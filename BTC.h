#ifndef BTC_H
#define BTC_H

#include "bitcoin/transaction.h"
#include "bitcoin/script.h"

#include <QByteArray>
#include <QString>
#include <vector>
#include <QMetaType>
#include <QPair>
#include <QHash>

#include <cstring>

namespace BTC
{

    /// Checks that the bitcoin lib has the correct
    /// endian settings for this platform. Will throw Exception on failure.
    extern void CheckBitcoinEndiannessCompiledCorrectly();

    enum Net {
        Invalid = 0,
        MainNet = 0x80,
        TestNet = 0xef
    };

    /// Helper class to glue QByteArray (which is very fast and efficient due to copy-on-write)
    /// to bitcoin's std::vector usage.
    /// This class also allows expressions like: ByteArray a = { opcode1, opcode2 } + somebytearray + { moreopcodes };
    /// Or: byteArray << OP_CODE1 << OP_CODE2 << someVal << etc...
    /// NB: Do not use this to operate on C strings as they may not always be nul terminated. For that,
    /// use QByteArray or QString.
    typedef unsigned char Byte;
    struct ByteArray : public std::vector<Byte>
    {
        ByteArray();
        ByteArray(const std::vector<Byte> &);
        ByteArray(std::vector<Byte> &&);
        // TO DO: see about removing some of this boilerplate using either C++ subtleties or templates.
        ByteArray(const QByteArray &);
        ByteArray(const QString &);
        ByteArray(const char *s) { *this = s; } ///< Note: the nul byte is NOT copied into the buffer.
        ByteArray(const std::initializer_list<Byte> &);

        ByteArray toHex() const; ///< returns Hex encoded (non-reversed)
        QByteArray toQHex() const; ///< returns Hex encoded (non-reversed)
        QString toHexStr() const { return QString(toQHex()); } ///< returns Hex encoded (non-reversed)

        /// Convenienct to obtain a pointer to [0]. If not empty, points to the same
        /// place as .begin().  If empty, will return a pointer to a static buffer
        /// with a 0 in it. Thus, a valid pointer is always returned.
        /// As such, don't rely on this to always == .begin() (it won't if empty)
        Byte *data(); ///< unsafe.
        const Byte* constData() const; ///< same notes as .data()

        /// convenience interop -- note that there may not be a nul byte at the end!
        char *charData() { return reinterpret_cast<char *>(data()); }
        /// convenience interop -- note that there may not be a nul byte at the end!
        const char *constCharData() const { return reinterpret_cast<const char *>(constData()); }

        // compat with Qt's int lengths
        int length() const { return int(size()); }
        // compat with QByteArray
        bool isEmpty() const { return empty(); }

        // TO DO: see about removing some of this boilerplate using either C++ subtleties or templates.
        ByteArray operator+(const std::vector<Byte> & o) const;
        ByteArray operator+(const QByteArray & o) const;
        ByteArray operator+(const QString &) const;
        ByteArray operator+(const char *s) const { return *this + QByteArray(s); }
        ByteArray operator+(const std::initializer_list<Byte> &) const;
        ByteArray & operator+=(const std::vector<Byte> & b);
        ByteArray & operator+=(const QByteArray &);
        ByteArray & operator+=(const QString &);
        ByteArray & operator+=(const std::initializer_list<Byte> &);
        ByteArray & operator+=(const char *s) { return *this += QByteArray(s); } ///< C-string terminating nul byte is NOT copied into the buffer!
        ByteArray & operator=(const std::vector<Byte> &o);
        ByteArray & operator=(const QByteArray &);
        ByteArray & operator=(const QString &);
        ByteArray & operator=(const std::initializer_list<Byte> &);
        ByteArray & operator=(const char *s) { return *this = QByteArray(s); }
        template <typename T>
        ByteArray & operator<<(const T &t) { return (*this) += t; }
        ByteArray & operator<<(bitcoin::opcodetype c) { return (*this) << Byte(c); } ///< append an op-code to this array
        ByteArray & operator<<(Byte c); ///< append any byte to this array
        operator QByteArray() const; ///< convenienct cast to QByteArray. Involves a full copy.
    };


    struct Address
    {
        enum Kind {
            Invalid = 0, P2PKH = 1, P2SH = 2
        };

        Address() {}
        Address(const QString &legacyAddress);
        Address(const char *legacy) { *this = legacy; }
        Address(const QByteArray &legacy) { *this = legacy; }

        QByteArray hash160() const { return h160; }

        Kind kind() const;

        bool isTestNet() const { return net == TestNet; }

        bool isValid() const;


        /// Returns the ElectrumX 'scripthash', bitcoin hex encoded.
        /// (Which is reversed because of bitcoin's way of encoding hex).
        /// The results of this function get cached.
        QByteArray toHashX() const;

        /// Returns the bitcoin script bytes as would be used in a spending transaction.
        /// (not reversed)
        /// The results of this function do not get cached.
        /// Note the return is a ByteArray and not a QByteArray.
        ByteArray toScript() const;
        /// Same as toScript(), but returns the data as a CScript object (bitcoin data structure
        /// for use with CTransaction et al in the txOut )
        bitcoin::CScript toCScript() const;
        /// Returns the bitcoin script bytes as would be used in a spending transaction,
        /// hashed once with sha256. (not reversed)
        /// The results of this function do not get cached
        /// Note the return is a ByteArray and not a QByteArray.
        ByteArray toScriptHash() const;

        /// If isValid, returns the legacy address string, base58 encoded
        /// Returns null string on error.
        QString toString() const;

        /// test any string to see if it's a valid address for the specified network
        static bool isValid(const QString &legacyAddress, Net = MainNet);

        Address & operator=(const QString &legacy) { return (*this = Address::fromString(legacy)); }
        Address & operator=(const char *legacy) { return (*this = QString(legacy)); }
        Address & operator=(const QByteArray &legacy) { return (*this = QString(legacy)); }

        bool operator==(const Address & o) const { return net == o.net && verByte == o.verByte && h160 == o.h160; }
        bool operator!=(const Address & o) const { return !(*this == o); }
        /// less operator: for map support and also so that it sorts like the text address would.
        bool operator<(const Address & o) const {
            if (isValid() && o.isValid()) {
                if (int cmp = std::memcmp(h160.constData(), o.h160.constData(), size_t(h160.length())); cmp < 0)
                    return true;
                else if (0==cmp)
                    return net < o.net ? true : (net==o.net ? verByte < o.verByte : false);
            }
            return false;
        }
        bool operator<=(const Address & o) const { return *this < o || *this == o; }

    private:
        Net net = BTC::Invalid;
        quint8 verByte = 99;
        QByteArray h160;
        mutable QByteArray cachedHashX;
        static Address fromString(const QString &legacy);
    public:
        static bool test();
    };

    /// for Qt QSet support of type Address
    inline uint qHash(const Address &key) {
        if (key.isValid())
            return key.hash160().left(8).toUInt(nullptr, 16); // just take the first 4 bytes and convert them to uint
        return 0;
    }

    struct UTXO {
        static constexpr int validTxidLength = (256/8)*2; ///< any txid not of this length (64) is immediately invalid.
        static constexpr quint32 invalidN = 0xffffffff;
    protected:
        /// hex encoded uint256 hash of its tx. (64 characters). Do not set these directly, instead use operator= etc below.
        QString _txid;
        quint32 _n = invalidN;
    public:
        inline const QString & txid() const { return _txid; }
        inline quint32 n() const { return _n; }

        /// construct an invalid utxo
        UTXO() {}
        /// construct a UTXO from a bitcoin::COutPoint
        UTXO(const bitcoin::COutPoint &cop) { *this = cop; }
        /// if hash is not 256 bit encoded hex, will be inValid()
        UTXO(const QString & prevoutHash, quint32 prevoutN) { setCheck(prevoutHash, prevoutN); }
        /// if "hash:N" is not "256 bit encoded hex:UInt", will be inValid()
        UTXO(const QString & prevoutHash_Colon_N) { setCheck(prevoutHash_Colon_N); }

        inline bool isValid() const { return _txid.length() == validTxidLength && _n != invalidN; }
        inline UTXO & clear() { _txid = QString(); _n = invalidN; return *this; }

        /// assign from a bitcoin::COutPoint. Not terribly efficient but fast enough for now.
        inline UTXO & operator=(const bitcoin::COutPoint & c) { return *this = c.ToQString(); }
        /// parses prevouthash:N, if ok, sets class to valid state and saves values, otherwise class becomes invalid.
        inline UTXO & operator=(const QString &prevOutN) { return setCheck(prevOutN); }

        inline bool operator<(const UTXO &b) const {
            if (isValid() && b.isValid()) {
                if (int cmp = _txid.compare(b._txid); cmp < 0)
                    return true;
                else if (0==cmp)
                    return _n < b._n;
                //else ...
            }
            return false;
        }
        inline bool operator<=(const UTXO &b) const { return *this < b || *this == b; }
        inline bool operator==(const UTXO &o) const { return _n == o._n && _txid == o._txid; }
        inline bool operator!=(const UTXO &o) const { return !(*this == o); }

        /// will only accept if the hash is valid hex, otherwise will leave this class in "Invalid" state
        UTXO & setCheck(const QString &prevoutHash, quint32 n);
        UTXO & setCheck(const QString &prevoutHash_Colon_N);

        bitcoin::COutPoint toCOutPoint() const;
        /// convert to prevouthash:N, returns a null string if !isValid()
        QString toString() const;

        static void test();
    };

    /// for Qt QSet support of type UTXO
    inline uint qHash(const UTXO &key, uint seed = 0) {
        if (key.isValid())
            return ::qHash(QPair<quint32, quint32>(key.txid().left(8).toUInt(nullptr, 16) , key.n()), seed);
            //return key.txid().left(8).toUInt(nullptr, 16) + key.n();
        return 0;
    }

    /// Make a bitcoin unsigned tx. Returns the total amount sent (sum of outputs). Note that
    /// no enforcement is done to make sure the tx is sane, so be sure to pass valid UTXO and Address values,
    /// as well as amounts >= 546 sats.  If any of the UTXOs !.isValid or any of the addresses !.isValid,
    /// or any of the outputs are below 546 sats, the resulting tx is empty and 0 is returned.
    extern
    quint64 MakeUnsignedTransaction(bitcoin::CMutableTransaction & tx,
                                    const QList<UTXO> & inputs, const QList<QPair<Address, quint64> > & outputs,
                                    quint32 nLockTime = 0);


} // end namespace

Q_DECLARE_METATYPE(BTC::Address);

#endif // BTC_H
