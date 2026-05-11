// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_BASE_H_
#define COMPONENTS_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/general_names.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parse_name.h"

// This namespace defines a base set of functions for parsing and accessing
// X.509 certificate data.
namespace x509_certificate_model {

struct NotPresent : std::monostate {};
struct Error : std::monostate {};
using OptionalStringOrError = std::variant<Error, NotPresent, std::string>;

class X509CertificateModelBase {
 public:
  X509CertificateModelBase(X509CertificateModelBase&& other);
  X509CertificateModelBase& operator=(X509CertificateModelBase&& other) =
      default;
  ~X509CertificateModelBase();

  X509CertificateModelBase(const X509CertificateModelBase&) = delete;
  X509CertificateModelBase& operator=(const X509CertificateModelBase&) = delete;

  // ---------------------------------------------------------------------------
  // These methods are always safe to call even if `cert_data` could not be
  // parsed.

  // Returns lower case hex SHA256 hash of the certificate data.
  std::string HashCertSHA256() const;

  // Get something that can be used as a title for the certificate, using the
  // following priority:
  //   subject commonName
  //   full subject
  //   dnsName or email address from subjectAltNames
  // If none of those are present, or certificate could not be parsed,
  // the hex SHA256 hash of the certificate data will be returned.
  std::string GetTitle() const;

  CRYPTO_BUFFER* cert_buffer() const { return cert_data_.get(); }
  bool is_valid() const { return parsed_successfully_; }

  // ---------------------------------------------------------------------------
  // The rest of the methods should only be called if `is_valid()` returns true.

  // Returns lower case hex SHA256 hash of the SPKI.
  std::string HashSpkiSHA256() const;

  std::string GetVersion() const;
  std::string GetSerialNumberHexified() const;

  std::string ProcessSecAlgorithmSignature() const;
  std::string ProcessSecAlgorithmSubjectPublicKey() const;
  std::string ProcessSecAlgorithmSignatureWrap() const;

  // Get the validity notBefore and notAfter times, returning true on success
  // or false on error in parsing or converting to a base::Time.
  bool GetTimes(base::Time* not_before, base::Time* not_after) const;

  // These methods return the issuer/subject commonName/orgName/orgUnitName
  // formatted as a string, if present. Returns NotPresent if the attribute
  // type was not present, or Error if there was a parsing error.
  // The Get{Issuer,Subject}CommonName methods return the last (most specific)
  // commonName, while the other methods return the first (most general) value.
  OptionalStringOrError GetIssuerCommonName() const;
  OptionalStringOrError GetIssuerOrgName() const;
  OptionalStringOrError GetIssuerOrgUnitName() const;
  OptionalStringOrError GetSubjectCommonName() const;
  OptionalStringOrError GetSubjectOrgName() const;
  OptionalStringOrError GetSubjectOrgUnitName() const;

 protected:
  // Parses the certificate data `cert_data` and populates
  // `tbs_certificate_tlv_`, `signature_algorithm_tlv_`, `signature_value_`,
  // `tbs_`, `subject_rdns_`, `issuer_rdns_`, and `extensions_`. If parsing
  // succeeds, `parsed_successfully_` is set to true.
  explicit X509CertificateModelBase(bssl::UniquePtr<CRYPTO_BUFFER> cert_data);

  bssl::der::Input tbs_certificate_tlv_;
  bssl::der::Input signature_algorithm_tlv_;
  bssl::der::BitString signature_value_;
  bssl::ParsedTbsCertificate tbs_;

  bssl::RDNSequence subject_rdns_;
  bssl::RDNSequence issuer_rdns_;

  // Parsed extensions from the certificate.
  std::vector<bssl::ParsedExtension> extensions_;

  // Parsed SubjectAltName extension.
  std::unique_ptr<bssl::GeneralNames> subject_alt_names_;

 private:
  // Parses the Extensions sequence, populating `extensions_` with each parsed
  // extension and `subject_alt_names_` if a SubjectAltName extension is
  // present.
  bool ParseExtensions(const bssl::der::Input& extensions_tlv);

  // Set to true by the class constructor after all parsing succeeds.
  bool parsed_successfully_ = false;

  bssl::UniquePtr<CRYPTO_BUFFER> cert_data_;
};

// Finds an attribute of the given OID type within a RelativeDistinguishedName.
// Returns NotPresent if the attribute was not found, Error if parsing failed,
// or the string value if found.
OptionalStringOrError FindAttributeOfType(
    bssl::der::Input oid,
    const bssl::RelativeDistinguishedName& rdn);

// Returns the value of the most general (first) name of the given OID type.
// Distinguished Names are specified in least to most specific order.
OptionalStringOrError FindFirstNameOfType(bssl::der::Input oid,
                                          const bssl::RDNSequence& rdns);

// Returns the value of the most specific (last) name of the given OID type.
// Distinguished Names are specified in least to most specific order.
OptionalStringOrError FindLastNameOfType(bssl::der::Input oid,
                                         const bssl::RDNSequence& rdns);

// Formats raw bytes as hex with `hex_separator` between bytes on the same
// line and `line_separator` every 16 bytes.
std::string ProcessRawBytesWithSeparators(base::span<const unsigned char> data,
                                          char hex_separator,
                                          char line_separator);

// Returns a string containing the dotted numeric form of `oid` prefixed by
// "OID.", or an empty string on error.
std::string OidToNumericString(bssl::der::Input oid);

// Parse an IA5String from a DER-encoded extension value.
std::optional<std::string> ProcessIA5String(bssl::der::Input extension_data);

// Parse a UserNotice DisplayText.
std::optional<std::string> ProcessUserNoticeDisplayText(CBS_ASN1_TAG tag,
                                                        bssl::der::Input value);

// Look up the l10n string resource ID for a common OID shared between
// platforms. Covers algorithm OIDs, some extended key usage OIDs, and
// certificate policy qualifier OIDs. Returns the IDS_CERT_* resource ID if
// found, or std::nullopt if the OID is not in the shared map.
std::optional<int> GetCommonOidStringId(bssl::der::Input oid);

// From RFC 3447:
// pkcs-1    OBJECT IDENTIFIER ::= {
//     iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) 1
// }
// rsaEncryption    OBJECT IDENTIFIER ::= { pkcs-1 1 }
inline constexpr uint8_t kPkcs1RsaEncryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                                  0x0d, 0x01, 0x01, 0x01};
// md2WithRSAEncryption       OBJECT IDENTIFIER ::= { pkcs-1 2 }
inline constexpr uint8_t kPkcs1Md2WithRsaEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x02};
// From RFC 2314: md4WithRSAEncryption OBJECT IDENTIFIER ::= { pkcs-1 3 }
inline constexpr uint8_t kPkcs1Md4WithRsaEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x03};
// md5WithRSAEncryption       OBJECT IDENTIFIER ::= { pkcs-1 4 }
inline constexpr uint8_t kPkcs1Md5WithRsaEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x04};
// sha1WithRSAEncryption      OBJECT IDENTIFIER ::= { pkcs-1 5 }
inline constexpr uint8_t kPkcs1Sha1WithRsaEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05};
// sha256WithRSAEncryption    OBJECT IDENTIFIER ::= { pkcs-1 11 }
inline constexpr uint8_t kPkcs1Sha256WithRsaEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
// sha384WithRSAEncryption    OBJECT IDENTIFIER ::= { pkcs-1 12 }
inline constexpr uint8_t kPkcs1Sha384WithRsaEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c};
// sha512WithRSAEncryption    OBJECT IDENTIFIER ::= { pkcs-1 13 }
inline constexpr uint8_t kPkcs1Sha512WithRsaEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0d};

// From RFC 3279:
//   ansi-X9-62  OBJECT IDENTIFIER ::= {
//            iso(1) member-body(2) us(840) 10045 }
//   id-ecSigType OBJECT IDENTIFIER  ::=  {
//        ansi-X9-62 signatures(4) }
//   ecdsa-with-SHA1  OBJECT IDENTIFIER ::= {
//        id-ecSigType 1 }
inline constexpr uint8_t kAnsiX962EcdsaWithSha1[] = {0x2a, 0x86, 0x48, 0xce,
                                                     0x3d, 0x04, 0x01};
// From RFC 5758:
//    ecdsa-with-SHA256 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
//            us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 2 }
inline constexpr uint8_t kAnsiX962EcdsaWithSha256[] = {0x2a, 0x86, 0x48, 0xce,
                                                       0x3d, 0x04, 0x03, 0x02};
//    ecdsa-with-SHA384 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
//            us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 3 }
inline constexpr uint8_t kAnsiX962EcdsaWithSha384[] = {0x2a, 0x86, 0x48, 0xce,
                                                       0x3d, 0x04, 0x03, 0x03};
//    ecdsa-with-SHA512 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
//            us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 4 }
inline constexpr uint8_t kAnsiX962EcdsaWithSha512[] = {0x2a, 0x86, 0x48, 0xce,
                                                       0x3d, 0x04, 0x03, 0x04};

// From RFC 3279:
//    ansi-X9-62 OBJECT IDENTIFIER ::=
//                            { iso(1) member-body(2) us(840) 10045 }
//    id-public-key-type OBJECT IDENTIFIER  ::= { ansi-X9.62 2 }
//    id-ecPublicKey OBJECT IDENTIFIER ::= { id-publicKeyType 1 }
inline constexpr uint8_t kAnsiX962EcPublicKey[] = {0x2a, 0x86, 0x48, 0xce,
                                                   0x3d, 0x02, 0x01};

// From RFC 5480:
//     secp256r1 OBJECT IDENTIFIER ::= {
//       iso(1) member-body(2) us(840) ansi-X9-62(10045) curves(3)
//       prime(1) 7 }
inline constexpr uint8_t kSecgEcSecp256r1[] = {0x2a, 0x86, 0x48, 0xce,
                                               0x3d, 0x03, 0x01, 0x07};
//     secp384r1 OBJECT IDENTIFIER ::= {
//       iso(1) identified-organization(3) certicom(132) curve(0) 34 }
inline constexpr uint8_t kSecgEcSecp384r1[] = {0x2b, 0x81, 0x04, 0x00, 0x22};
//     secp521r1 OBJECT IDENTIFIER ::= {
//       iso(1) identified-organization(3) certicom(132) curve(0) 35 }
inline constexpr uint8_t kSecgEcSecp521r1[] = {0x2b, 0x81, 0x04, 0x00, 0x23};

// From RFC 9881
inline constexpr uint8_t kOidAlgMldsa44[] = {OBJ_ENC_ML_DSA_44};
inline constexpr uint8_t kOidAlgMldsa65[] = {OBJ_ENC_ML_DSA_65};
inline constexpr uint8_t kOidAlgMldsa87[] = {OBJ_ENC_ML_DSA_87};

// The certificate viewer may be used to view client certificates, so use the
// relaxed parsing mode. See crbug.com/41347446 and crbug.com/41357486.
inline constexpr auto kNameStringHandling =
    bssl::X509NameAttribute::PrintableStringHandling::kAsUTF8Hack;

}  // namespace x509_certificate_model

#endif  // COMPONENTS_CERTIFICATE_MODEL_X509_CERTIFICATE_MODEL_BASE_H_
