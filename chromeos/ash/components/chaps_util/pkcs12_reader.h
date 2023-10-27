// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_PKCS12_READER_H_
#define CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_PKCS12_READER_H_

#include <cstdint>
#include <string>
#include <vector>

#include <nss/certt.h>
#include "base/component_export.h"
#include "base/containers/span.h"
#include "chromeos/ash/components/chaps_util/chaps_slot_session.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/pkcs7.h"

namespace chromeos {

// Used for logging, the values should never be reordered or reused.
enum class Pkcs12ReaderStatusCode {
  kSuccess = 0,
  kCreateKeyFailed = 1,
  kCertificateDataMissed = 2,
  kCreateCertFailed = 3,
  kKeyDataMissed = 4,
  kKeyExtractionFailed = 5,
  kChapsSessionMissed = 6,
  kPkcs12CertDerMissed = 7,
  kPkcs12CertDerFailed = 8,
  kPkcs12CertIssuerNameMissed = 9,
  kPkcs12CertIssuerDerNameFailed = 10,
  kPkcs12CertSubjectNameMissed = 11,
  kPkcs12CertSubjectNameDerFailed = 12,
  kPkcs12CertSerialNumberMissed = 13,
  kPkcs12CertSerialNumberDerFailed = 14,
  kKeyAttrDataMissing = 15,
  kFailureDuringCertImport = 16,
  kFailedToParsePkcs12Data = 17,
  kMissedPkcs12Data = 18,
  kPkcs12LabelCreationFailed = 19,
  kPkcs12FindCertsWithSubjectFailed = 20,
  kPkcs12NoValidCertificatesFound = 21,
  kPkcs12ErrorDuringExistingCertConversion = 22,
  kPkcs12NoNicknamesWasExtracted = 23,
  kPkcs12CanNotExtractDefaultLabelFromCert = 24,
  kPkcs12CanNotDecodeRawCert = 25,
  kPkcs12ReachedMaxAttemptForUniqueness = 26,
  kPkcs12MissedNickname = 27,
  kMissedSlotInfo = 28,
  kPkcs12NotSupportedKeyType = 29,
  kPkcs12CNExtractionFailed = 30,
  kPkcs12EcKeyExtractionFailed = 31,
  kPkcs12CkaIdExtractionFailed = 32,
  kPublicKeyExtractionFailed = 33,
  kRsaKeyExtractionFailed = 34,
  kRsaModulusExtractionFailed = 35,
  kPkcs12RsaModulusEmpty = 36,
  kPkcs12PKeyMissed = 37,
};

enum class Pkcs12ReaderCertSearchType {
  kDerType,
  kPlainType,
};

struct CertData {
  CertData();
  ~CertData();
  raw_ptr<X509> x509;
  std::string nickname;
};

struct COMPONENT_EXPORT(CHAPS_UTIL) KeyData {
  KeyData();
  KeyData(KeyData&&);
  KeyData& operator=(KeyData&&) = default;
  ~KeyData();
  bssl::UniquePtr<EVP_PKEY> key;
  std::vector<uint8_t> rsa_key_modulus_bytes;
  std::vector<uint8_t> ec_key_public_bytes;
  std::vector<uint8_t> common_name;
  std::vector<uint8_t> cka_id_value;
};

// Class helper for operations with X509 certificates data which are required
// for storing keys and certificates in Chaps.
class COMPONENT_EXPORT(CHAPS_UTIL) Pkcs12Reader {
 public:
  Pkcs12Reader() = default;

  virtual ~Pkcs12Reader() = default;

  // Populates key and certificates (`key`, `certs`) from the PKCS#12 object
  // `pkcs12_data` protected by the `password`. Returns status code.
  virtual Pkcs12ReaderStatusCode GetPkcs12KeyAndCerts(
      const std::vector<uint8_t>& pkcs12_data,
      const std::string& password,
      bssl::UniquePtr<EVP_PKEY>& key,
      bssl::UniquePtr<STACK_OF(X509)>& certs) const;

  // Populates der encoded certificate and its size (`cert_der`,
  // `cert_der_size`) from X509 (`cert`). Returns status code.
  virtual Pkcs12ReaderStatusCode GetDerEncodedCert(
      X509* cert,
      bssl::UniquePtr<uint8_t>& cert_der,
      int& cert_der_size) const;

  // Populates der encoded issuer name and its size (`issuer_name_data`) from
  // X509 (`cert`). `issuer_name_data` remains valid only as long as the cert is
  // alive because it is only referencing data. Returns status code.
  virtual Pkcs12ReaderStatusCode GetIssuerNameDer(
      X509* cert,
      base::span<const uint8_t>& issuer_name_data) const;

  // Populates der encoded subject name and its size (`subject_name_data`) from
  // X509 (`cert`). `subject_name_data` remains valid only as long as the cert
  // is alive because it is only referencing data. Returns status code.
  virtual Pkcs12ReaderStatusCode GetSubjectNameDer(
      X509* cert,
      base::span<const uint8_t>& subject_name_data) const;

  // Populates der encoded serial number and its size (`serial_number_der`,
  // `serial_number_der_size`) from X509 (`cert`). Returns status code.
  virtual Pkcs12ReaderStatusCode GetSerialNumberDer(
      X509* cert,
      bssl::UniquePtr<uint8_t>& serial_number_der,
      int& serial_number_der_size) const;

  // Populates label (`label`) from X509 (`cert`). Returns status code.
  virtual Pkcs12ReaderStatusCode GetLabel(X509* cert, std::string& label) const;

  // Converts BIGNUM (`bignum`) to bytes.
  virtual std::vector<uint8_t> BignumToBytes(const BIGNUM* bignum) const;

  // Fetches X509 certificates (`found_certs`) with required Subject name
  // (`required_subject_name`) from the provided slot (`slot`).
  // Returns status code.
  virtual Pkcs12ReaderStatusCode FindRawCertsWithSubject(
      PK11SlotInfo* slot,
      base::span<const uint8_t> required_subject_name,
      CERTCertificateList** found_certs) const;

  // Check if there are certificates with the same nickname (`nickname_in`) is
  // present in any PK11 slots. Set (`is_nickname_present`) to true or false.
  // Returns status code.
  virtual Pkcs12ReaderStatusCode IsCertWithNicknameInSlots(
      const std::string& nickname_in,
      bool& is_nickname_present) const;

  // Search if private key is already present in slot (`slot`) using
  // related X509 certificate (`cert`) and certificates type (`cert_type`).
  // Returns Pkcs12ReaderStatusCode::kSuccess
  // if key found, Pkcs12ReaderStatusCode::kKeyDataMissed if key is missed or
  // a status code.
  virtual Pkcs12ReaderStatusCode DoesKeyForCertExist(
      PK11SlotInfo* slot,
      const Pkcs12ReaderCertSearchType cert_type,
      const scoped_refptr<net::X509Certificate>& cert) const;

  // Calculates additional data which can be used for checking key to
  // certificate relation from (`key`). For RSA key it will be modulus. Returns
  // status code.
  virtual Pkcs12ReaderStatusCode EnrichKeyData(KeyData& key_data) const;

  // Check if certificate (`cert`) is related to private key which was used for
  // the calculation of key_data (`key_data`), sets boolean (`is_related`).
  // Returns status code.
  virtual Pkcs12ReaderStatusCode CheckRelation(const KeyData& key_data,
                                               X509* cert,
                                               bool& is_related) const;
  // Converts certificates data from DER representation (`der_cert_data`) and
  // (`der_cert_len`) to X509 (`x509`).
  // Returns status code.
  virtual Pkcs12ReaderStatusCode GetCertFromDerData(
      const unsigned char* der_cert_data,
      int der_cert_len,
      bssl::UniquePtr<X509>& x509) const;

 private:
  // Calculate (`modulus`) for RSA key extracted from (`pkey`).
  // Returns status code.
  virtual Pkcs12ReaderStatusCode GetRsaModulus(
      const bssl::UniquePtr<EVP_PKEY>& pkey,
      std::vector<uint8_t>& modulus) const;
};

}  // namespace chromeos

#endif  // CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_PKCS12_READER_H_
