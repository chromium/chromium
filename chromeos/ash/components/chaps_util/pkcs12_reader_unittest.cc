// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chromeos/ash/components/chaps_util/pkcs12_reader.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/x509.h"

namespace chromeos {
namespace {

const char kPkcs12FilePassword[] = "12345";

// Custom X509 object creation allows to avoid calls to X509_free()
// after every test where X509 objects are required.
struct X509Deleter {
  void operator()(X509* cert) { X509_free(cert); }
};
using ScopedX509 = std::unique_ptr<X509, X509Deleter>;
ScopedX509 X509New() {
  return ScopedX509(X509_new());
}

// Custom X509_NAME object creation allows to avoid calls to X509_NAME_free()
// after every test where X509_NAME objects are required.
struct X509NameDeleter {
  void operator()(X509_NAME* name) { X509_NAME_free(name); }
};
using ScopedX509_NAME = std::unique_ptr<X509_NAME, X509NameDeleter>;
ScopedX509_NAME X509NameNew() {
  return ScopedX509_NAME(X509_NAME_new());
}

// Custom BIGNUM object with object's deleter after the test is finished.
struct BIGNUMDeleter {
  void operator()(BIGNUM* num) { BN_free(num); }
};
using ScopedBIGNUM = std::unique_ptr<BIGNUM, BIGNUMDeleter>;
ScopedBIGNUM BIGNUMNew() {
  return ScopedBIGNUM(BN_new());
}

scoped_refptr<net::X509Certificate> GetTestCert() {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
}

KeyData BuildBasicKeyData() {
  KeyData key_data;
  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  key_data.key = std::move(pkey);
  return key_data;
}

bssl::UniquePtr<EVP_PKEY> GenerateRsaKey() {
  EVP_PKEY* key_ptr = EVP_PKEY_new();
  EVP_PKEY_CTX* ctx;
  ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
  EVP_PKEY_keygen_init(ctx);
  EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048);
  EVP_PKEY_keygen(ctx, &key_ptr);
  EVP_PKEY_CTX_free(ctx);
  return bssl::UniquePtr<EVP_PKEY>(key_ptr);
}

// Tests for testing methods in chaps_util_helper.cc
// ChapsUtilImplTest is testing successful import and values, these tests
// are mainly checking errors handling.
class Pkcs12ReaderTest : public ::testing::Test {
 public:
  Pkcs12ReaderTest() { pkcs12Reader_ = std::make_unique<Pkcs12Reader>(); }
  Pkcs12ReaderTest(const Pkcs12ReaderTest&) = delete;
  Pkcs12ReaderTest& operator=(const Pkcs12ReaderTest&) = delete;
  ~Pkcs12ReaderTest() override = default;

  Pkcs12ReaderStatusCode GetSerialNumberDer(X509* cert) {
    int serial_number_der_size;
    bssl::UniquePtr<uint8_t> serial_number_der;
    return pkcs12Reader_->GetSerialNumberDer(cert, serial_number_der,
                                             serial_number_der_size);
  }

  Pkcs12ReaderStatusCode GetIssuerNameDer(X509* cert) {
    base::span<const uint8_t> issuer_name_data;
    return pkcs12Reader_->GetIssuerNameDer(cert, issuer_name_data);
  }

  Pkcs12ReaderStatusCode GetSubjectNameDer(X509* cert) {
    base::span<const uint8_t> subject_name_data;
    return pkcs12Reader_->GetSubjectNameDer(cert, subject_name_data);
  }

  Pkcs12ReaderStatusCode GetDerEncodedCert(X509* cert) {
    int cert_der_size;
    bssl::UniquePtr<uint8_t> cert_der_ptr;
    return pkcs12Reader_->GetDerEncodedCert(cert, cert_der_ptr, cert_der_size);
  }

  Pkcs12ReaderStatusCode GetLabel(X509* cert) {
    std::string label;
    return pkcs12Reader_->GetLabel(cert, label);
  }

  void SetFieldToX509Name(X509_NAME* X509_name,
                          const char field[],
                          unsigned char value[]) {
    X509_NAME_add_entry_by_txt(X509_name,
                               /*field=*/field,
                               /*type=*/MBSTRING_ASC,
                               /*bytes=*/value,
                               /*len=*/-1,
                               /*loc=*/-1,
                               /*set=*/0);
  }

  void SetOrgDataWithoutCNToX509Name(X509_NAME* X509_name) {
    // Country
    unsigned char country_name[] = "DE";
    SetFieldToX509Name(X509_name, "C", country_name);

    // Company/Organization
    unsigned char org_name[] = "Test company";
    SetFieldToX509Name(X509_name, "O", org_name);
  }

  void SetOrgDataToX509Name(X509_NAME* X509_name) {
    // Country name and Company/Organization name
    SetOrgDataWithoutCNToX509Name(X509_name);

    // Common name
    SetFieldToX509Name(X509_name, "CN", common_name_);
  }

  unsigned char common_name_[12] = "common name";

 protected:
  std::unique_ptr<Pkcs12Reader> pkcs12Reader_;
};

TEST_F(Pkcs12ReaderTest, EmptyBigNumReturnsEmptyVector) {
  ScopedBIGNUM bignum = BIGNUMNew();
  BN_zero(bignum.get());
  std::vector<uint8_t> expected_empty_vector({});

  EXPECT_EQ(pkcs12Reader_->BignumToBytes(bignum.get()), expected_empty_vector);
}

TEST_F(Pkcs12ReaderTest, MaxBigNumConvertedCorrectly) {
  ScopedBIGNUM bignum = BIGNUMNew();
  BN_set_u64(bignum.get(), 0xFFFFFFFFFFFFFFFF);
  std::vector<uint8_t> expected_data({
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
      0xFF,
  });

  std::vector<uint8_t> bignum_bytes =
      pkcs12Reader_->BignumToBytes(bignum.get());

  EXPECT_EQ(bignum_bytes, expected_data);
}

TEST_F(Pkcs12ReaderTest, BigNumZeroConvertedToEmptyVector) {
  ScopedBIGNUM bignum = BIGNUMNew();
  BN_set_u64(bignum.get(), 0x00000000000000);
  std::vector<uint8_t> expected_data({});

  std::vector<uint8_t> bignum_bytes =
      pkcs12Reader_->BignumToBytes(bignum.get());

  EXPECT_EQ(bignum_bytes, expected_data);
}

TEST_F(Pkcs12ReaderTest, BigNumWithFrontZerosConvertedCorrectly) {
  ScopedBIGNUM bignum = BIGNUMNew();
  BN_set_u64(bignum.get(), 0x00000000000100);
  std::vector<uint8_t> expected_data({0x01, 0x00});

  std::vector<uint8_t> bignum_bytes =
      pkcs12Reader_->BignumToBytes(bignum.get());

  EXPECT_EQ(bignum_bytes, expected_data);
}

TEST_F(Pkcs12ReaderTest, EmptyBigNumConvertedCorrectly) {
  ScopedBIGNUM bignum = BIGNUMNew();
  std::vector<uint8_t> expected_data({});

  std::vector<uint8_t> bignum_bytes =
      pkcs12Reader_->BignumToBytes(bignum.get());

  EXPECT_EQ(bignum_bytes, expected_data);
}

TEST_F(Pkcs12ReaderTest, NullptrBigNumConvertedCorrectly) {
  std::vector<uint8_t> expected_data({});

  std::vector<uint8_t> bignum_bytes = pkcs12Reader_->BignumToBytes(nullptr);

  EXPECT_EQ(bignum_bytes, expected_data);
}

TEST_F(Pkcs12ReaderTest, GetSerialNumberDer) {
  // Empty certificate, operation will fail.
  {
    X509* cert = nullptr;

    Pkcs12ReaderStatusCode result = GetSerialNumberDer(cert);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kCertificateDataMissed);
  }

  // Empty serial number, operation will succeed.
  {
    ScopedX509 cert = X509New();

    Pkcs12ReaderStatusCode result = GetSerialNumberDer(cert.get());
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }

  // Certificate with normal serial number, operation will succeed.
  // Check only import success, values are checked in ChapsUtilImplTest.
  {
    ScopedX509 cert = X509New();
    ASN1_INTEGER_set(X509_get_serialNumber(cert.get()), 1);
    int serial_number_der_size;
    bssl::UniquePtr<uint8_t> serial_number_der;

    Pkcs12ReaderStatusCode result = pkcs12Reader_->GetSerialNumberDer(
        cert.get(), serial_number_der, serial_number_der_size);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }
}

TEST_F(Pkcs12ReaderTest, GetIssuerNameDer) {
  // Empty certificate, operation will fail.
  {
    Pkcs12ReaderStatusCode result = GetIssuerNameDer(nullptr);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kCertificateDataMissed);
  }

  // Empty object for the issuer, operation will succeed.
  {
    ScopedX509 cert = X509New();

    Pkcs12ReaderStatusCode result = GetIssuerNameDer(cert.get());
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }

  // Certificate with normal issuer name, operation will succeed.
  // Check only import success, values are checked in ChapsUtilImplTest.
  {
    ScopedX509 cert = X509New();
    ScopedX509_NAME issuer = X509NameNew();

    // This only sets org name, country and common name.
    SetOrgDataToX509Name(issuer.get());
    X509_set_issuer_name(cert.get(), issuer.get());
    base::span<const uint8_t> issuer_name_data;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->GetIssuerNameDer(cert.get(), issuer_name_data);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }
}

TEST_F(Pkcs12ReaderTest, FindRawCertsWithSubject) {
  crypto::ScopedTestNSSDB nss_test_db_;

  // Empty slot, operation will fail.
  {
    PK11SlotInfo* slot = nullptr;
    base::span<const uint8_t> required_subject_name;
    CERTCertificateList* found_certs = nullptr;

    Pkcs12ReaderStatusCode result = pkcs12Reader_->FindRawCertsWithSubject(
        slot, required_subject_name, &found_certs);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kMissedSlotInfo);
    EXPECT_EQ(found_certs, nullptr);
  }

  // Not defined subject name, operation will fail.
  {
    PK11SlotInfo* slot = nss_test_db_.slot();
    base::span<const uint8_t> required_subject_name;
    CERTCertificateList* found_certs = nullptr;

    Pkcs12ReaderStatusCode result = pkcs12Reader_->FindRawCertsWithSubject(
        slot, required_subject_name, &found_certs);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12CertSubjectNameMissed);
    EXPECT_EQ(found_certs, nullptr);
  }

  // Fake subject name, operation will succeed, but list will be still not
  // defined because call is using empty nss_test_db.
  {
    PK11SlotInfo* slot = nss_test_db_.slot();
    const uint8_t subject_name[] = "subject_name";
    base::span<const uint8_t> required_subject_name{subject_name,
                                                    std::size(subject_name)};
    CERTCertificateList* found_certs = nullptr;

    Pkcs12ReaderStatusCode result = pkcs12Reader_->FindRawCertsWithSubject(
        slot, required_subject_name, &found_certs);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
    EXPECT_EQ(found_certs, nullptr);
  }
}

TEST_F(Pkcs12ReaderTest, GetSubjectNameDer) {
  // Empty certificate, operation will fail.
  {
    Pkcs12ReaderStatusCode result = GetSubjectNameDer(nullptr);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kCertificateDataMissed);
  }

  // Empty object for the subject name, operation will succeed.
  {
    ScopedX509 cert = X509New();

    Pkcs12ReaderStatusCode result = GetSubjectNameDer(cert.get());
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }

  // Certificate with normal subject name, operation will succeed.
  // Check only import success, values are checked in ChapsUtilImplTest.
  {
    ScopedX509 cert = X509New();
    ScopedX509_NAME subject = X509NameNew();

    // This only sets org name, country and common name.
    SetOrgDataToX509Name(subject.get());
    X509_set_subject_name(cert.get(), subject.get());
    base::span<const uint8_t> subject_name_data;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->GetSubjectNameDer(cert.get(), subject_name_data);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }
}

TEST_F(Pkcs12ReaderTest, GetDerEncodedCert) {
  // No certificate, operation will fail.
  {
    Pkcs12ReaderStatusCode result = GetDerEncodedCert(nullptr);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kCertificateDataMissed);
  }

  // Empty certificate, operation will fail.
  {
    ScopedX509 cert = X509New();

    Pkcs12ReaderStatusCode result = GetDerEncodedCert(cert.get());
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12CertDerFailed);
  }
}

TEST_F(Pkcs12ReaderTest, GetPkcs12KeyAndCerts) {
  // No pkcs12 data, operation will fail.
  {
    bssl::UniquePtr<EVP_PKEY> key;
    bssl::UniquePtr<STACK_OF(X509)> certs;
    const std::vector<uint8_t>& pkcs12_data = {};

    Pkcs12ReaderStatusCode result = pkcs12Reader_->GetPkcs12KeyAndCerts(
        pkcs12_data, kPkcs12FilePassword, key, certs);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kMissedPkcs12Data);
  }

  // Wrong pkcs12 data's, operation will fail.
  {
    bssl::UniquePtr<EVP_PKEY> key;
    bssl::UniquePtr<STACK_OF(X509)> certs;
    const std::vector<uint8_t>& wrong_pkcs12_data = {0, 0, 0, 0, 0,
                                                     0, 0, 0, 0, 0};

    Pkcs12ReaderStatusCode result = pkcs12Reader_->GetPkcs12KeyAndCerts(
        wrong_pkcs12_data, kPkcs12FilePassword, key, certs);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kFailedToParsePkcs12Data);
  }

  // Not testing for normal case and for password, those cases are tested from
  // higher level chaps_util_impl_unittest.cc
}

TEST_F(Pkcs12ReaderTest, GetLabel) {
  // Certificate is NULL, operation will fail.
  {
    Pkcs12ReaderStatusCode result = GetLabel(nullptr);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kCertificateDataMissed);
  }

  // Certificate with present friendly name (alias), operation will
  // succeed. Label is taken from friendly name (alias).
  {
    ScopedX509 cert = X509New();
    ScopedX509_NAME subject = X509NameNew();

    // This only sets org name, country and common name.
    SetOrgDataToX509Name(subject.get());
    X509_set_subject_name(cert.get(), subject.get());
    unsigned char alias[20] = "new alias";
    std::string expected_label(reinterpret_cast<char*>(alias));
    X509_alias_set1(cert.get(), alias, std::size(alias));
    std::string label;

    Pkcs12ReaderStatusCode result = pkcs12Reader_->GetLabel(cert.get(), label);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
    EXPECT_EQ(label.c_str(), expected_label);
  }

  // Certificate without friendly name (alias) and with empty subject name,
  // operation will fail with kPkcs12CNExtractionFailed.
  {
    ScopedX509 cert = X509New();
    std::string label;

    Pkcs12ReaderStatusCode result = pkcs12Reader_->GetLabel(cert.get(), label);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12CNExtractionFailed);
  }

  // Certificate without friendly name (alias), but with subject name,
  // operation will succeed. Label will be taken from CN.
  {
    ScopedX509 cert = X509New();
    ScopedX509_NAME subject = X509NameNew();

    // This only sets org name, country and common name.
    SetOrgDataToX509Name(subject.get());
    X509_set_subject_name(cert.get(), subject.get());
    std::string expected_label(reinterpret_cast<char*>(common_name_));
    std::string label;

    Pkcs12ReaderStatusCode result = pkcs12Reader_->GetLabel(cert.get(), label);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
    EXPECT_EQ(label.c_str(), expected_label);
  }

  // Certificate without friendly name (alias) and with empty subject name,
  // operation will fail with kPkcs12CNExtractionFailed.
  {
    ScopedX509 cert = X509New();
    std::string label;

    Pkcs12ReaderStatusCode result = pkcs12Reader_->GetLabel(cert.get(), label);
    EXPECT_TRUE(label.empty());
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12CNExtractionFailed);
  }
}

TEST_F(Pkcs12ReaderTest, isCertsWithNicknamesInSlots) {
  // Empty nickname, isCertsWithNicknamesInSlot returns fail.
  {
    std::string nickname = "";
    bool is_nickname_present = false;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->IsCertWithNicknameInSlots(nickname, is_nickname_present);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12MissedNickname);
  }

  // Not defined nickname, isCertsWithNicknamesInSlot returns fail.
  {
    std::string not_defined_nickname;
    bool is_nickname_present = false;

    Pkcs12ReaderStatusCode result = pkcs12Reader_->IsCertWithNicknameInSlots(
        not_defined_nickname, is_nickname_present);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12MissedNickname);
  }

  // Not testing normal case, it is tested with certificate in
  // chaps_util_impl_unittest.cc
}

TEST_F(Pkcs12ReaderTest, DoesKeyForCertExist) {
  crypto::ScopedTestNSSDB nss_test_db_;
  auto cert_type = Pkcs12ReaderCertSearchType::kPlainType;

  // Empty certificate, operation will fail.
  {
    PK11SlotInfo* slot = nullptr;
    scoped_refptr<net::X509Certificate> cert = nullptr;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->DoesKeyForCertExist(slot, cert_type, cert);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kCertificateDataMissed);
  }

  // Empty slot, operation will fail.
  {
    PK11SlotInfo* slot = nullptr;
    scoped_refptr<net::X509Certificate> cert = GetTestCert();

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->DoesKeyForCertExist(slot, cert_type, cert);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kMissedSlotInfo);
  }

  // Slot and cert provided, it is a test slot, so key will be missed.
  {
    scoped_refptr<net::X509Certificate> cert = GetTestCert();

    Pkcs12ReaderStatusCode result = pkcs12Reader_->DoesKeyForCertExist(
        nss_test_db_.slot(), cert_type, cert);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kKeyDataMissed);
  }
}

TEST_F(Pkcs12ReaderTest, DoesKeyForDerCertExist) {
  crypto::ScopedTestNSSDB nss_test_db_;
  auto cert_type = Pkcs12ReaderCertSearchType::kDerType;

  // Empty certificate, operation will fail.
  {
    PK11SlotInfo* slot = nullptr;
    scoped_refptr<net::X509Certificate> cert = nullptr;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->DoesKeyForCertExist(slot, cert_type, cert);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kCertificateDataMissed);
  }

  // Empty slot, operation will fail.
  {
    PK11SlotInfo* slot = nullptr;
    scoped_refptr<net::X509Certificate> cert = GetTestCert();

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->DoesKeyForCertExist(slot, cert_type, cert);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kMissedSlotInfo);
  }

  // Slot and cert provided, it is a test slot, so key will be missed.
  {
    scoped_refptr<net::X509Certificate> cert = GetTestCert();

    Pkcs12ReaderStatusCode result = pkcs12Reader_->DoesKeyForCertExist(
        nss_test_db_.slot(), cert_type, cert);
    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kKeyDataMissed);
  }
}

TEST_F(Pkcs12ReaderTest, EnrichKeyData) {
  crypto::ScopedTestNSSDB nss_test_db_;

  // Empty key_data, operation will fail.
  {
    KeyData key_data;

    Pkcs12ReaderStatusCode result = pkcs12Reader_->EnrichKeyData(key_data);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kKeyDataMissed);
  }

  // RSA is NULL. Operation will fail.
  {
    KeyData key_data;
    key_data.key = GenerateRsaKey();
    EVP_PKEY_assign_RSA(key_data.key.get(), nullptr);

    Pkcs12ReaderStatusCode result = pkcs12Reader_->EnrichKeyData(key_data);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kRsaKeyExtractionFailed);
  }

  // RSA is present, but empty. Operation will fail.
  {
    KeyData key_data;
    key_data.key = GenerateRsaKey();
    EVP_PKEY_assign_RSA(key_data.key.get(), RSA_new());

    Pkcs12ReaderStatusCode result = pkcs12Reader_->EnrichKeyData(key_data);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12RsaModulusEmpty);
  }

  // Normal RSA key, operation will succeed.
  {
    KeyData key_data;
    key_data.key = GenerateRsaKey();

    Pkcs12ReaderStatusCode result = pkcs12Reader_->EnrichKeyData(key_data);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
  }
}

TEST_F(Pkcs12ReaderTest, CheckRelation) {
  // Empty key_data, operation will fail.
  {
    KeyData key_data;
    ScopedX509 cert = X509New();
    bool is_related = true;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->CheckRelation(key_data, cert.get(), is_related);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kKeyDataMissed);
    EXPECT_FALSE(is_related);
  }

  // Empty cert, operation will fail.
  {
    KeyData key_data = BuildBasicKeyData();
    X509* cert = nullptr;
    bool is_related = true;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->CheckRelation(key_data, cert, is_related);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kCertificateDataMissed);
    EXPECT_FALSE(is_related);
  }

  // No key RSA data and no EC key data, operation will fail.
  {
    KeyData key_data = BuildBasicKeyData();
    ScopedX509 cert = X509New();
    bool is_related = true;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->CheckRelation(key_data, cert.get(), is_related);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12NotSupportedKeyType);
    EXPECT_FALSE(is_related);
  }

  // RSA key modulus is present in key_data, but cert has no public key,
  // operation will fail.
  {
    KeyData key_data = BuildBasicKeyData();
    key_data.rsa_key_modulus_bytes = {1, 2, 3, 4};  // Not a real modulus.
    ScopedX509 cert = X509New();
    bool is_related = true;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->CheckRelation(key_data, cert.get(), is_related);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12PKeyMissed);
    EXPECT_FALSE(is_related);
  }

  // RSA key modulus is present in the key_data, cert has a public key,
  // public key is not the same as private key, operation will fail.
  {
    KeyData key_data = BuildBasicKeyData();
    key_data.rsa_key_modulus_bytes = {1, 2, 3, 4};  // Not a real modulus.
    ScopedX509 cert = X509New();
    bssl::UniquePtr<EVP_PKEY> pub_key = GenerateRsaKey();
    ;
    X509_set_pubkey(cert.get(), pub_key.get());
    bool is_related = true;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->CheckRelation(key_data, cert.get(), is_related);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12NoValidCertificatesFound);
    EXPECT_FALSE(is_related);
  }

  // RSA key modulus is present in the key_data, cert has a public key,
  // public key modulus is the same as private key modulus, operation will
  // succeed.
  {
    KeyData key_data;
    key_data.key = GenerateRsaKey();
    const RSA* rsa_key = EVP_PKEY_get0_RSA(key_data.key.get());
    key_data.rsa_key_modulus_bytes =
        pkcs12Reader_->BignumToBytes(RSA_get0_n(rsa_key));
    ScopedX509 cert = X509New();
    // Set a cert's key to the same value as private key, so modulus will
    // be equal.
    X509_set_pubkey(cert.get(), key_data.key.get());
    bool is_related = false;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->CheckRelation(key_data, cert.get(), is_related);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kSuccess);
    EXPECT_TRUE(is_related);
  }
}

TEST_F(Pkcs12ReaderTest, GetCertFromDerData) {
  // Cert data is empty, valid cert can not be extracted.
  {
    const unsigned char* der_cert_data = nullptr;
    int der_cert_len = 1;
    bssl::UniquePtr<X509> x509;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->GetCertFromDerData(der_cert_data, der_cert_len, x509);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12NoValidCertificatesFound);
  }

  // Cert len is 0, valid cert can not be extracted.
  {
    const unsigned char der_cert_data[1024] = {1, 2};
    int der_cert_len = 0;
    bssl::UniquePtr<X509> x509;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->GetCertFromDerData(der_cert_data, der_cert_len, x509);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kPkcs12NoValidCertificatesFound);
  }

  // Cert data is not empty, but it is not a valid cert, cert creation failed.
  {
    const unsigned char der_cert_data[1024] = {1, 2};
    int der_cert_len = 2;
    bssl::UniquePtr<X509> x509;

    Pkcs12ReaderStatusCode result =
        pkcs12Reader_->GetCertFromDerData(der_cert_data, der_cert_len, x509);

    EXPECT_EQ(result, Pkcs12ReaderStatusCode::kCreateCertFailed);
  }
}

}  // namespace
}  // namespace chromeos
