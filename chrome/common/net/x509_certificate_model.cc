// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include <string_view>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "crypto/sha2.h"
#include "net/base/ip_address.h"
#include "net/cert/ct_objects_extractor.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/certificate_policies.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parse_name.h"
#include "third_party/boringssl/src/pki/parse_values.h"
#include "third_party/boringssl/src/pki/parser.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"
#include "third_party/boringssl/src/pki/verify_signed_data.h"
#include "ui/base/l10n/l10n_util.h"

namespace x509_certificate_model {

namespace {

// 2.5.4.46 NAME 'dnQualifier'
constexpr uint8_t kTypeDnQualifierOid[] = {0x55, 0x04, 0x2e};
// 2.5.4.15 NAME 'businessCategory'
constexpr uint8_t kTypeBusinessCategory[] = {0x55, 0x04, 0x0f};
// 2.5.4.17 NAME 'postalCode'
constexpr uint8_t kTypePostalCode[] = {0x55, 0x04, 0x11};

// TODO(mattm): we can probably just remove these RFC 1274 OIDs.
//
// ccitt is {0} but not explicitly defined in the RFC 1274.
// RFC 1274:
// data OBJECT IDENTIFIER ::= {ccitt 9}
// pss OBJECT IDENTIFIER ::= {data 2342}
// ucl OBJECT IDENTIFIER ::= {pss 19200300}
// pilot OBJECT IDENTIFIER ::= {ucl 100}
// pilotAttributeType OBJECT IDENTIFIER ::= {pilot 1}
// userid ::= {pilotAttributeType 1}
constexpr uint8_t kRFC1274UidOid[] = {0x09, 0x92, 0x26, 0x89, 0x93,
                                      0xf2, 0x2c, 0x64, 0x01, 0x01};
// rfc822Mailbox :: = {pilotAttributeType 3}
constexpr uint8_t kRFC1274MailOid[] = {0x09, 0x92, 0x26, 0x89, 0x93,
                                       0xf2, 0x2c, 0x64, 0x01, 0x03};

// jurisdictionLocalityName (OID: 1.3.6.1.4.1.311.60.2.1.1)
constexpr uint8_t kEVJurisdictionLocalityName[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x3c, 0x02, 0x01, 0x01};
// jurisdictionStateOrProvinceName (OID: 1.3.6.1.4.1.311.60.2.1.2)
constexpr uint8_t kEVJurisdictionStateOrProvinceName[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x3c, 0x02, 0x01, 0x02};
// jurisdictionCountryName (OID: 1.3.6.1.4.1.311.60.2.1.3)
constexpr uint8_t kEVJurisdictionCountryName[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x3c, 0x02, 0x01, 0x03};

// From RFC 5280:
//     id-ce-issuerAltName OBJECT IDENTIFIER ::=  { id-ce 18 }
// In dotted notation: 2.5.29.18
constexpr uint8_t kIssuerAltNameOid[] = {0x55, 0x1d, 0x12};
// From RFC 5280:
//     id-ce-subjectDirectoryAttributes OBJECT IDENTIFIER ::=  { id-ce 9 }
// In dotted notation: 2.5.29.9
constexpr uint8_t kSubjectDirectoryAttributesOid[] = {0x55, 0x1d, 0x09};

// From RFC 3447:
// pkcs-1    OBJECT IDENTIFIER ::= {
//     iso(1) member-body(2) us(840) rsadsi(113549) pkcs(1) 1
// }
// rsaEncryption    OBJECT IDENTIFIER ::= { pkcs-1 1 }
constexpr uint8_t kPkcs1RsaEncryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                           0x0d, 0x01, 0x01, 0x01};
// md2WithRSAEncryption       OBJECT IDENTIFIER ::= { pkcs-1 2 }
constexpr uint8_t kPkcs1Md2WithRsaEncryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                                  0x0d, 0x01, 0x01, 0x02};
// From RFC 2314: md4WithRSAEncryption OBJECT IDENTIFIER ::= { pkcs-1 3 }
constexpr uint8_t kPkcs1Md4WithRsaEncryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                                  0x0d, 0x01, 0x01, 0x03};
// md5WithRSAEncryption       OBJECT IDENTIFIER ::= { pkcs-1 4 }
constexpr uint8_t kPkcs1Md5WithRsaEncryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                                  0x0d, 0x01, 0x01, 0x04};
// sha1WithRSAEncryption      OBJECT IDENTIFIER ::= { pkcs-1 5 }
constexpr uint8_t kPkcs1Sha1WithRsaEncryption[] = {0x2a, 0x86, 0x48, 0x86, 0xf7,
                                                   0x0d, 0x01, 0x01, 0x05};
// sha256WithRSAEncryption    OBJECT IDENTIFIER ::= { pkcs-1 11 }
constexpr uint8_t kPkcs1Sha256WithRsaEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
// sha384WithRSAEncryption    OBJECT IDENTIFIER ::= { pkcs-1 12 }
constexpr uint8_t kPkcs1Sha384WithRsaEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c};
// sha512WithRSAEncryption    OBJECT IDENTIFIER ::= { pkcs-1 13 }
constexpr uint8_t kPkcs1Sha512WithRsaEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0d};
// From RFC 3279:
//   ansi-X9-62  OBJECT IDENTIFIER ::= {
//            iso(1) member-body(2) us(840) 10045 }
//   id-ecSigType OBJECT IDENTIFIER  ::=  {
//        ansi-X9-62 signatures(4) }
//   ecdsa-with-SHA1  OBJECT IDENTIFIER ::= {
//        id-ecSigType 1 }
constexpr uint8_t kAnsiX962EcdsaWithSha1[] = {0x2a, 0x86, 0x48, 0xce,
                                              0x3d, 0x04, 0x01};
// From RFC 5758:
//    ecdsa-with-SHA256 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
//            us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 2 }
constexpr uint8_t kAnsiX962EcdsaWithSha256[] = {0x2a, 0x86, 0x48, 0xce,
                                                0x3d, 0x04, 0x03, 0x02};
//    ecdsa-with-SHA384 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
//            us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 3 }
constexpr uint8_t kAnsiX962EcdsaWithSha384[] = {0x2a, 0x86, 0x48, 0xce,
                                                0x3d, 0x04, 0x03, 0x03};
//    ecdsa-with-SHA512 OBJECT IDENTIFIER ::= { iso(1) member-body(2)
//            us(840) ansi-X9-62(10045) signatures(4) ecdsa-with-SHA2(3) 4 }
constexpr uint8_t kAnsiX962EcdsaWithSha512[] = {0x2a, 0x86, 0x48, 0xce,
                                                0x3d, 0x04, 0x03, 0x04};
// From RFC 3279:
//    ansi-X9-62 OBJECT IDENTIFIER ::=
//                            { iso(1) member-body(2) us(840) 10045 }
//    id-public-key-type OBJECT IDENTIFIER  ::= { ansi-X9.62 2 }
//    id-ecPublicKey OBJECT IDENTIFIER ::= { id-publicKeyType 1 }
constexpr uint8_t kAnsiX962EcPublicKey[] = {0x2a, 0x86, 0x48, 0xce,
                                            0x3d, 0x02, 0x01};
// From RFC 5480:
//     secp256r1 OBJECT IDENTIFIER ::= {
//       iso(1) member-body(2) us(840) ansi-X9-62(10045) curves(3)
//       prime(1) 7 }
constexpr uint8_t kSecgEcSecp256r1[] = {0x2a, 0x86, 0x48, 0xce,
                                        0x3d, 0x03, 0x01, 0x07};
//     secp384r1 OBJECT IDENTIFIER ::= {
//       iso(1) identified-organization(3) certicom(132) curve(0) 34 }
constexpr uint8_t kSecgEcSecp384r1[] = {0x2b, 0x81, 0x04, 0x00, 0x22};
//     secp521r1 OBJECT IDENTIFIER ::= {
//       iso(1) identified-organization(3) certicom(132) curve(0) 35 }
constexpr uint8_t kSecgEcSecp512r1[] = {0x2b, 0x81, 0x04, 0x00, 0x23};

// Old Netscape OIDs. Do we still need all these?
// #define NETSCAPE_OID 0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x42
// #define NETSCAPE_CERT_EXT NETSCAPE_OID, 0x01
//
// CONST_OID nsExtCertType[] = { NETSCAPE_CERT_EXT, 0x01 };
constexpr uint8_t kNetscapeCertificateTypeOid[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                                   0xf8, 0x42, 0x01, 0x01};

// CONST_OID nsExtBaseURL[] = { NETSCAPE_CERT_EXT, 0x02 };
constexpr uint8_t kNetscapeBaseURLOid[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                           0xf8, 0x42, 0x01, 0x02};

// CONST_OID nsExtRevocationURL[] = { NETSCAPE_CERT_EXT, 0x03 };
constexpr uint8_t kNetscapeRevocationURLOid[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                                 0xf8, 0x42, 0x01, 0x03};

// CONST_OID nsExtCARevocationURL[] = { NETSCAPE_CERT_EXT, 0x04 };
constexpr uint8_t kNetscapeCARevocationURLOid[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                                   0xf8, 0x42, 0x01, 0x04};

// CONST_OID nsExtCACertURL[] = { NETSCAPE_CERT_EXT, 0x06 };
constexpr uint8_t kNetscapeCACertURLOid[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                             0xf8, 0x42, 0x01, 0x06};

// CONST_OID nsExtCertRenewalURL[] = { NETSCAPE_CERT_EXT, 0x07 };
constexpr uint8_t kNetscapeRenewalURLOid[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                              0xf8, 0x42, 0x01, 0x07};

// CONST_OID nsExtCAPolicyURL[] = { NETSCAPE_CERT_EXT, 0x08 };
constexpr uint8_t kNetscapeCAPolicyURLOid[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                               0xf8, 0x42, 0x01, 0x08};

// CONST_OID nsExtSSLServerName[] = { NETSCAPE_CERT_EXT, 0x0c };
constexpr uint8_t kNetscapeSSLServerNameOid[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                                 0xf8, 0x42, 0x01, 0x0c};

// CONST_OID nsExtComment[] = { NETSCAPE_CERT_EXT, 0x0d };
constexpr uint8_t kNetscapeCommentOid[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                           0xf8, 0x42, 0x01, 0x0d};

// CONST_OID nsExtLostPasswordURL[] = { NETSCAPE_CERT_EXT, 0x0e };
constexpr uint8_t kNetscapeLostPasswordURLOid[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                                   0xf8, 0x42, 0x01, 0x0e};

// CONST_OID nsExtCertRenewalTime[] = { NETSCAPE_CERT_EXT, 0x0f };
constexpr uint8_t kNetscapeRenewalTimeOid[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                               0xf8, 0x42, 0x01, 0x0f};

constexpr uint8_t kNetscapeServerGatedCrypto[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                                  0xf8, 0x42, 0x04, 0x01};

// Microsoft OIDs. Do we still need all these?
//
// 1.3.6.1.4.1.311.20.2
constexpr uint8_t kMsCertExtCerttype[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                          0x82, 0x37, 0x14, 0x02};

// 1.3.6.1.4.1.311.21.1
constexpr uint8_t kMsCertsrvCaVersion[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                           0x82, 0x37, 0x15, 0x01};

// 1.3.6.1.4.1.311.20.2.3
constexpr uint8_t kMsNtPrincipalName[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                          0x82, 0x37, 0x14, 0x02, 0x03};

// 1.3.6.1.4.1.311.25.1
constexpr uint8_t kMsNtdsReplication[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                          0x82, 0x37, 0x19, 0x01};

// 1.3.6.1.4.1.311.21.7
constexpr uint8_t kMsCertTemplate[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                       0x82, 0x37, 0x15, 0x07};

// 1.3.6.1.4.1.311.2.1.21
constexpr uint8_t kEkuMsIndividualCodeSigning[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x02, 0x01, 0x15};

// 1.3.6.1.4.1.311.2.1.22
constexpr uint8_t kEkuMsCommercialCodeSigning[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x02, 0x01, 0x16};

// 1.3.6.1.4.1.311.10.3.1
constexpr uint8_t kEkuMsTrustListSigning[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                              0x82, 0x37, 0x0a, 0x03, 0x01};

// 1.3.6.1.4.1.311.10.3.2
constexpr uint8_t kEkuMsTimeStamping[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                          0x82, 0x37, 0x0a, 0x03, 0x02};

// 1.3.6.1.4.1.311.10.3.3
constexpr uint8_t kEkuMsServerGatedCrypto[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                               0x82, 0x37, 0x0a, 0x03, 0x03};

// 1.3.6.1.4.1.311.10.3.4
constexpr uint8_t kEkuMsEncryptingFileSystem[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                                  0x82, 0x37, 0x0a, 0x03, 0x04};

// 1.3.6.1.4.1.311.10.3.4.1
constexpr uint8_t kEkuMsFileRecovery[] = {0x2b, 0x06, 0x01, 0x04, 0x01, 0x82,
                                          0x37, 0x0a, 0x03, 0x04, 0x01};

// 1.3.6.1.4.1.311.10.3.5
constexpr uint8_t kEkuMsWindowsHardwareDriverVerification[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x0a, 0x03, 0x05};

// 1.3.6.1.4.1.311.10.3.10
constexpr uint8_t kEkuMsQualifiedSubordination[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x0a, 0x03, 0x0a};

// 1.3.6.1.4.1.311.10.3.11
constexpr uint8_t kEkuMsKeyRecovery[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                         0x82, 0x37, 0x0a, 0x03, 0x0b};

// 1.3.6.1.4.1.311.10.3.12
constexpr uint8_t kEkuMsDocumentSigning[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                             0x82, 0x37, 0x0a, 0x03, 0x0c};

// 1.3.6.1.4.1.311.10.3.13
constexpr uint8_t kEkuMsLifetimeSigning[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                             0x82, 0x37, 0x0a, 0x03, 0x0d};

// 1.3.6.1.4.1.311.20.2.2
constexpr uint8_t kEkuMsSmartCardLogon[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                            0x82, 0x37, 0x14, 0x02, 0x02};

// 1.3.6.1.4.1.311.21.6
constexpr uint8_t kEkuMsKeyRecoveryAgent[] = {0x2b, 0x06, 0x01, 0x04, 0x01,
                                              0x82, 0x37, 0x15, 0x06};

// The certificate viewer may be used to view client certificates, so use the
// relaxed parsing mode. See crbug.com/770323 and crbug.com/788655.
constexpr auto kNameStringHandling =
    bssl::X509NameAttribute::PrintableStringHandling::kAsUTF8Hack;

std::string ProcessRawBytesWithSeparators(base::span<const unsigned char> data,
                                          char hex_separator,
                                          char line_separator) {
  // Each input byte creates two output hex characters + a space or newline,
  // except for the last byte.
  std::string ret;
  size_t kMin = 0U;

  if (data.empty()) {
    return std::string();
  }

  ret.reserve(std::max(kMin, data.size() * 3 - 1));

  for (size_t i = 0; i < data.size(); ++i) {
    base::AppendHexEncodedByte(data[i], ret);
    if (i + 1 < data.size()) {
      ret.push_back(((i + 1) % 16) ? hex_separator : line_separator);
    }
  }
  return ret;
}

std::string ProcessRawBytes(base::span<const uint8_t> data) {
  return ProcessRawBytesWithSeparators(data, ' ', '\n');
}

OptionalStringOrError FindAttributeOfType(
    bssl::der::Input oid,
    const bssl::RelativeDistinguishedName& rdn) {
  // In X.509, RelativeDistinguishedName is a Set, so order has no meaning, and
  // generally only has one element anyway. Just traverse in encoded order.
  for (const bssl::X509NameAttribute& name_attribute : rdn) {
    if (name_attribute.type == oid) {
      std::string rv;
      if (!name_attribute.ValueAsStringWithUnsafeOptions(kNameStringHandling,
                                                         &rv)) {
        return Error();
      }
      // TODO(mattm): do something about newlines (or other control chars)?
      return rv;
    }
  }
  return NotPresent();
}

// Returns the value of the most general name of |oid| type.
// Distinguished Names are specified in least to most specific.
OptionalStringOrError FindFirstNameOfType(bssl::der::Input oid,
                                          const bssl::RDNSequence& rdns) {
  for (const bssl::RelativeDistinguishedName& rdn : rdns) {
    OptionalStringOrError r = FindAttributeOfType(oid, rdn);
    if (!absl::holds_alternative<NotPresent>(r))
      return r;
  }
  return NotPresent();
}

// Returns the value of the most specific name of |oid| type.
// Distinguished Names are specified in least to most specific.
OptionalStringOrError FindLastNameOfType(bssl::der::Input oid,
                                         const bssl::RDNSequence& rdns) {
  for (const bssl::RelativeDistinguishedName& rdn : base::Reversed(rdns)) {
    OptionalStringOrError r = FindAttributeOfType(oid, rdn);
    if (!absl::holds_alternative<NotPresent>(r))
      return r;
  }
  return NotPresent();
}

// Returns a string containing the dotted numeric form of |oid| prefixed by
// "OID.", or an empty string on error.
std::string OidToNumericString(bssl::der::Input oid) {
  CBS cbs;
  CBS_init(&cbs, oid.data(), oid.size());
  bssl::UniquePtr<char> text(CBS_asn1_oid_to_text(&cbs));
  if (!text)
    return std::string();
  return std::string("OID.") + text.get();
}

constexpr auto kOidStringMap = base::MakeFixedFlatMap<bssl::der::Input, int>({
    // Distinguished Name fields:
    {bssl::der::Input(bssl::kTypeCommonNameOid), IDS_CERT_OID_AVA_COMMON_NAME},
    {bssl::der::Input(bssl::kTypeStateOrProvinceNameOid),
     IDS_CERT_OID_AVA_STATE_OR_PROVINCE},
    {bssl::der::Input(bssl::kTypeOrganizationNameOid),
     IDS_CERT_OID_AVA_ORGANIZATION_NAME},
    {bssl::der::Input(bssl::kTypeOrganizationUnitNameOid),
     IDS_CERT_OID_AVA_ORGANIZATIONAL_UNIT_NAME},
    {bssl::der::Input(kTypeDnQualifierOid), IDS_CERT_OID_AVA_DN_QUALIFIER},
    {bssl::der::Input(bssl::kTypeCountryNameOid),
     IDS_CERT_OID_AVA_COUNTRY_NAME},
    {bssl::der::Input(bssl::kTypeSerialNumberOid),
     IDS_CERT_OID_AVA_SERIAL_NUMBER},
    {bssl::der::Input(bssl::kTypeLocalityNameOid), IDS_CERT_OID_AVA_LOCALITY},
    {bssl::der::Input(bssl::kTypeDomainComponentOid), IDS_CERT_OID_AVA_DC},
    {bssl::der::Input(kRFC1274MailOid), IDS_CERT_OID_RFC1274_MAIL},
    {bssl::der::Input(kRFC1274UidOid), IDS_CERT_OID_RFC1274_UID},
    {bssl::der::Input(bssl::kTypeEmailAddressOid),
     IDS_CERT_OID_PKCS9_EMAIL_ADDRESS},

    // Extended Validation (EV) name fields:
    {bssl::der::Input(kTypeBusinessCategory), IDS_CERT_OID_BUSINESS_CATEGORY},
    {bssl::der::Input(kEVJurisdictionLocalityName),
     IDS_CERT_OID_EV_INCORPORATION_LOCALITY},
    {bssl::der::Input(kEVJurisdictionStateOrProvinceName),
     IDS_CERT_OID_EV_INCORPORATION_STATE},
    {bssl::der::Input(kEVJurisdictionCountryName),
     IDS_CERT_OID_EV_INCORPORATION_COUNTRY},
    {bssl::der::Input(bssl::kTypeStreetAddressOid),
     IDS_CERT_OID_AVA_STREET_ADDRESS},
    {bssl::der::Input(kTypePostalCode), IDS_CERT_OID_AVA_POSTAL_CODE},

    // Algorithm fields:
    {bssl::der::Input(kPkcs1RsaEncryption), IDS_CERT_OID_PKCS1_RSA_ENCRYPTION},
    {bssl::der::Input(kPkcs1Md2WithRsaEncryption),
     IDS_CERT_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION},
    {bssl::der::Input(kPkcs1Md4WithRsaEncryption),
     IDS_CERT_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION},
    {bssl::der::Input(kPkcs1Md5WithRsaEncryption),
     IDS_CERT_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION},
    {bssl::der::Input(kPkcs1Sha1WithRsaEncryption),
     IDS_CERT_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION},
    {bssl::der::Input(kPkcs1Sha256WithRsaEncryption),
     IDS_CERT_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION},
    {bssl::der::Input(kPkcs1Sha384WithRsaEncryption),
     IDS_CERT_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION},
    {bssl::der::Input(kPkcs1Sha512WithRsaEncryption),
     IDS_CERT_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION},
    {bssl::der::Input(kAnsiX962EcdsaWithSha1),
     IDS_CERT_OID_ANSIX962_ECDSA_SHA1_SIGNATURE},
    {bssl::der::Input(kAnsiX962EcdsaWithSha256),
     IDS_CERT_OID_ANSIX962_ECDSA_SHA256_SIGNATURE},
    {bssl::der::Input(kAnsiX962EcdsaWithSha384),
     IDS_CERT_OID_ANSIX962_ECDSA_SHA384_SIGNATURE},
    {bssl::der::Input(kAnsiX962EcdsaWithSha512),
     IDS_CERT_OID_ANSIX962_ECDSA_SHA512_SIGNATURE},
    {bssl::der::Input(kAnsiX962EcPublicKey),
     IDS_CERT_OID_ANSIX962_EC_PUBLIC_KEY},
    {bssl::der::Input(kSecgEcSecp256r1), IDS_CERT_OID_SECG_EC_SECP256R1},
    {bssl::der::Input(kSecgEcSecp384r1), IDS_CERT_OID_SECG_EC_SECP384R1},
    {bssl::der::Input(kSecgEcSecp512r1), IDS_CERT_OID_SECG_EC_SECP521R1},

    // Extension fields (including details of extensions):
    {bssl::der::Input(kNetscapeCertificateTypeOid), IDS_CERT_EXT_NS_CERT_TYPE},
    {bssl::der::Input(kNetscapeBaseURLOid), IDS_CERT_EXT_NS_CERT_BASE_URL},
    {bssl::der::Input(kNetscapeRevocationURLOid),
     IDS_CERT_EXT_NS_CERT_REVOCATION_URL},
    {bssl::der::Input(kNetscapeCARevocationURLOid),
     IDS_CERT_EXT_NS_CA_REVOCATION_URL},
    {bssl::der::Input(kNetscapeRenewalURLOid),
     IDS_CERT_EXT_NS_CERT_RENEWAL_URL},
    {bssl::der::Input(kNetscapeCAPolicyURLOid), IDS_CERT_EXT_NS_CA_POLICY_URL},
    {bssl::der::Input(kNetscapeSSLServerNameOid),
     IDS_CERT_EXT_NS_SSL_SERVER_NAME},
    {bssl::der::Input(kNetscapeCommentOid), IDS_CERT_EXT_NS_COMMENT},
    {bssl::der::Input(kNetscapeLostPasswordURLOid),
     IDS_CERT_EXT_NS_LOST_PASSWORD_URL},
    {bssl::der::Input(kNetscapeRenewalTimeOid),
     IDS_CERT_EXT_NS_CERT_RENEWAL_TIME},
    {bssl::der::Input(kSubjectDirectoryAttributesOid),
     IDS_CERT_X509_SUBJECT_DIRECTORY_ATTR},
    {bssl::der::Input(bssl::kSubjectKeyIdentifierOid),
     IDS_CERT_X509_SUBJECT_KEYID},
    {bssl::der::Input(bssl::kAuthorityKeyIdentifierOid),
     IDS_CERT_X509_AUTH_KEYID},
    {bssl::der::Input(bssl::kKeyUsageOid), IDS_CERT_X509_KEY_USAGE},
    {bssl::der::Input(bssl::kSubjectAltNameOid),
     IDS_CERT_X509_SUBJECT_ALT_NAME},
    {bssl::der::Input(kIssuerAltNameOid), IDS_CERT_X509_ISSUER_ALT_NAME},
    {bssl::der::Input(bssl::kBasicConstraintsOid),
     IDS_CERT_X509_BASIC_CONSTRAINTS},
    {bssl::der::Input(bssl::kNameConstraintsOid),
     IDS_CERT_X509_NAME_CONSTRAINTS},
    {bssl::der::Input(bssl::kCrlDistributionPointsOid),
     IDS_CERT_X509_CRL_DIST_POINTS},
    {bssl::der::Input(bssl::kCertificatePoliciesOid),
     IDS_CERT_X509_CERT_POLICIES},
    {bssl::der::Input(bssl::kPolicyMappingsOid), IDS_CERT_X509_POLICY_MAPPINGS},
    {bssl::der::Input(bssl::kPolicyConstraintsOid),
     IDS_CERT_X509_POLICY_CONSTRAINTS},
    {bssl::der::Input(bssl::kExtKeyUsageOid), IDS_CERT_X509_EXT_KEY_USAGE},
    {bssl::der::Input(bssl::kAuthorityInfoAccessOid),
     IDS_CERT_X509_AUTH_INFO_ACCESS},
    {bssl::der::Input(bssl::kCpsPointerId),
     IDS_CERT_PKIX_CPS_POINTER_QUALIFIER},
    {bssl::der::Input(bssl::kUserNoticeId),
     IDS_CERT_PKIX_USER_NOTICE_QUALIFIER},
    {bssl::der::Input(net::ct::kEmbeddedSCTOid), IDS_CERT_X509_SCT_LIST},

    // Extended Key Usages:
    {bssl::der::Input(bssl::kAnyEKU), IDS_CERT_EKU_ANY_EKU},
    {bssl::der::Input(bssl::kServerAuth),
     IDS_CERT_EKU_TLS_WEB_SERVER_AUTHENTICATION},
    {bssl::der::Input(bssl::kClientAuth),
     IDS_CERT_EKU_TLS_WEB_CLIENT_AUTHENTICATION},
    {bssl::der::Input(bssl::kCodeSigning), IDS_CERT_EKU_CODE_SIGNING},
    {bssl::der::Input(bssl::kEmailProtection), IDS_CERT_EKU_EMAIL_PROTECTION},
    {bssl::der::Input(bssl::kTimeStamping), IDS_CERT_EKU_TIME_STAMPING},
    {bssl::der::Input(bssl::kOCSPSigning), IDS_CERT_EKU_OCSP_SIGNING},
    {bssl::der::Input(kNetscapeServerGatedCrypto),
     IDS_CERT_EKU_NETSCAPE_INTERNATIONAL_STEP_UP},

    // Microsoft oids:
    {bssl::der::Input(kMsCertExtCerttype), IDS_CERT_EXT_MS_CERT_TYPE},
    {bssl::der::Input(kMsCertsrvCaVersion), IDS_CERT_EXT_MS_CA_VERSION},
    {bssl::der::Input(kMsNtPrincipalName), IDS_CERT_EXT_MS_NT_PRINCIPAL_NAME},
    {bssl::der::Input(kMsNtdsReplication), IDS_CERT_EXT_MS_NTDS_REPLICATION},
    {bssl::der::Input(bssl::kMSApplicationPoliciesOid),
     IDS_CERT_EXT_MS_APP_POLICIES},
    {bssl::der::Input(kMsCertTemplate), IDS_CERT_EXT_MS_CERT_TEMPLATE},
    {bssl::der::Input(kEkuMsIndividualCodeSigning),
     IDS_CERT_EKU_MS_INDIVIDUAL_CODE_SIGNING},
    {bssl::der::Input(kEkuMsCommercialCodeSigning),
     IDS_CERT_EKU_MS_COMMERCIAL_CODE_SIGNING},
    {bssl::der::Input(kEkuMsTrustListSigning),
     IDS_CERT_EKU_MS_TRUST_LIST_SIGNING},
    {bssl::der::Input(kEkuMsTimeStamping), IDS_CERT_EKU_MS_TIME_STAMPING},
    {bssl::der::Input(kEkuMsServerGatedCrypto),
     IDS_CERT_EKU_MS_SERVER_GATED_CRYPTO},
    {bssl::der::Input(kEkuMsEncryptingFileSystem),
     IDS_CERT_EKU_MS_ENCRYPTING_FILE_SYSTEM},
    {bssl::der::Input(kEkuMsFileRecovery), IDS_CERT_EKU_MS_FILE_RECOVERY},
    {bssl::der::Input(kEkuMsWindowsHardwareDriverVerification),
     IDS_CERT_EKU_MS_WINDOWS_HARDWARE_DRIVER_VERIFICATION},
    {bssl::der::Input(kEkuMsQualifiedSubordination),
     IDS_CERT_EKU_MS_QUALIFIED_SUBORDINATION},
    {bssl::der::Input(kEkuMsKeyRecovery), IDS_CERT_EKU_MS_KEY_RECOVERY},
    {bssl::der::Input(kEkuMsDocumentSigning), IDS_CERT_EKU_MS_DOCUMENT_SIGNING},
    {bssl::der::Input(kEkuMsLifetimeSigning), IDS_CERT_EKU_MS_LIFETIME_SIGNING},
    {bssl::der::Input(kEkuMsSmartCardLogon), IDS_CERT_EKU_MS_SMART_CARD_LOGON},
    {bssl::der::Input(kEkuMsKeyRecoveryAgent),
     IDS_CERT_EKU_MS_KEY_RECOVERY_AGENT},
});

std::optional<std::string> GetOidText(bssl::der::Input oid) {
  const auto i = kOidStringMap.find(oid);
  if (i != kOidStringMap.end())
    return l10n_util::GetStringUTF8(i->second);
  return std::nullopt;
}

std::string GetOidTextOrNumeric(bssl::der::Input oid) {
  std::optional<std::string> oid_text = GetOidText(oid);
  return oid_text ? *oid_text : OidToNumericString(oid);
}

std::string ProcessRDN(const bssl::RelativeDistinguishedName& rdn) {
  std::string rv;
  // In X.509, RelativeDistinguishedName is a Set, so "last" has no meaning,
  // and generally only has one element anyway.  Just traverse in encoded
  // order.
  for (const bssl::X509NameAttribute& name_attribute : rdn) {
    std::string oid_text = GetOidTextOrNumeric(name_attribute.type);
    if (oid_text.empty())
      return std::string();
    rv += oid_text;
    std::string value;
    if (!name_attribute.ValueAsStringWithUnsafeOptions(kNameStringHandling,
                                                       &value)) {
      return std::string();
    }
    rv += " = ";
    if (name_attribute.type == bssl::der::Input(bssl::kTypeCommonNameOid)) {
      value = ProcessIDN(value);
    }
    // TODO(mattm): do something about newlines (or other control chars)?
    rv += value;
    rv += "\n";
  }
  return rv;
}

// Note: This was called ProcessName in the x509_certificate_model_nss impl.
OptionalStringOrError RDNSequenceToStringMultiLine(
    const bssl::RDNSequence& rdns) {
  if (rdns.empty())
    return NotPresent();

  std::string rv;
  // Note: this has high level similarity to net::ConvertToRFC2253, but
  // this one is multi-line, and prints in reverse order, and has a different
  // set of oids that it has printable names for, and different handling of
  // unprintable values, and IDN processing...
  for (const bssl::RelativeDistinguishedName& rdn : base::Reversed(rdns)) {
    std::string rdn_value = ProcessRDN(rdn);
    if (rdn_value.empty())
      return Error();
    rv += rdn_value;
  }
  return rv;
}

std::optional<std::string> ProcessIA5String(bssl::der::Input extension_data) {
  bssl::der::Input value;
  bssl::der::Parser parser(extension_data);
  std::string rv;
  if (!parser.ReadTag(CBS_ASN1_IA5STRING, &value) || parser.HasMore() ||
      !bssl::der::ParseIA5String(value, &rv)) {
    return std::nullopt;
  }
  // TODO(mattm): do something about newlines (or other control chars)?
  return rv;
}

// Returns a comma-separated string of the strings in |string_map| for the bits
// in |bitfield| that are set.
// string_map may contain -1 for reserved positions that should not be set.
std::optional<std::string> ProcessBitField(bssl::der::BitString bitfield,
                                           base::span<const int> string_map,
                                           char separator) {
  std::string rv;
  for (size_t i = 0; i < string_map.size(); ++i) {
    if (bitfield.AssertsBit(i)) {
      int string_id = string_map[i];
      // TODO(mattm): is returning an error here correct? Or should it encode
      // some generic string like "reserved bit N set"?
      if (string_id < 0)
        return std::nullopt;
      if (!rv.empty())
        rv += separator;
      rv += l10n_util::GetStringUTF8(string_id);
    }
  }
  // TODO(mattm): should it be an error if bitfield asserts bits beyond |len|?
  // Or encode them with some generic string like "bit N set"?
  return rv;
}

// Returns nullopt on error, or empty string if no bits were set.
std::optional<std::string> ProcessBitStringValue(
    bssl::der::Input value,
    base::span<const int> string_map,
    char separator) {
  std::optional<bssl::der::BitString> decoded =
      bssl::der::ParseBitString(value);
  if (!decoded) {
    return std::nullopt;
  }
  return ProcessBitField(decoded.value(), string_map, separator);
}

std::optional<std::string> ProcessBitStringExtension(
    bssl::der::Input extension_data,
    base::span<const int> string_map,
    char separator) {
  bssl::der::Input value;
  bssl::der::Parser parser(extension_data);
  if (!parser.ReadTag(CBS_ASN1_BITSTRING, &value) || parser.HasMore()) {
    return std::nullopt;
  }

  return ProcessBitStringValue(value, string_map, separator);
}

std::optional<std::string> ProcessNSCertTypeExtension(
    bssl::der::Input extension_data) {
  static const int usage_strings[] = {
      IDS_CERT_USAGE_SSL_CLIENT,
      IDS_CERT_USAGE_SSL_SERVER,
      IDS_CERT_EXT_NS_CERT_TYPE_EMAIL,
      IDS_CERT_USAGE_OBJECT_SIGNER,
      -1,  // reserved
      IDS_CERT_USAGE_SSL_CA,
      IDS_CERT_EXT_NS_CERT_TYPE_EMAIL_CA,
      IDS_CERT_USAGE_OBJECT_SIGNER,
  };
  return ProcessBitStringExtension(extension_data, usage_strings, '\n');
}

std::optional<std::string> ProcessKeyUsageExtension(
    bssl::der::Input extension_data) {
  static const int usage_strings[] = {
      IDS_CERT_X509_KEY_USAGE_SIGNING,
      IDS_CERT_X509_KEY_USAGE_NONREP,
      IDS_CERT_X509_KEY_USAGE_ENCIPHERMENT,
      IDS_CERT_X509_KEY_USAGE_DATA_ENCIPHERMENT,
      IDS_CERT_X509_KEY_USAGE_KEY_AGREEMENT,
      IDS_CERT_X509_KEY_USAGE_CERT_SIGNER,
      IDS_CERT_X509_KEY_USAGE_CRL_SIGNER,
      IDS_CERT_X509_KEY_USAGE_ENCIPHER_ONLY,
      IDS_CERT_X509_KEY_USAGE_DECIPHER_ONLY,
  };
  std::optional<std::string> rv =
      ProcessBitStringExtension(extension_data, usage_strings, '\n');
  if (rv && rv->empty()) {
    // RFC 5280 4.2.1.3:
    // When the keyUsage extension appears in a certificate, at least one of
    // the bits MUST be set to 1.
    return std::nullopt;
  }
  return rv;
}

std::optional<std::string> ProcessBasicConstraints(
    bssl::der::Input extension_data) {
  bssl::ParsedBasicConstraints basic_constraints;
  if (!bssl::ParseBasicConstraints(extension_data, &basic_constraints)) {
    return std::nullopt;
  }

  std::string rv;
  if (basic_constraints.is_ca)
    rv = l10n_util::GetStringUTF8(IDS_CERT_X509_BASIC_CONSTRAINT_IS_CA);
  else
    rv = l10n_util::GetStringUTF8(IDS_CERT_X509_BASIC_CONSTRAINT_IS_NOT_CA);
  rv += '\n';
  if (basic_constraints.is_ca) {
    std::u16string depth;
    if (!basic_constraints.has_path_len) {
      depth = l10n_util::GetStringUTF16(
          IDS_CERT_X509_BASIC_CONSTRAINT_PATH_LEN_UNLIMITED);
    } else {
      depth = base::FormatNumber(basic_constraints.path_len);
    }
    rv += l10n_util::GetStringFUTF8(IDS_CERT_X509_BASIC_CONSTRAINT_PATH_LEN,
                                    depth);
  }
  return rv;
}

std::optional<std::string> ProcessExtKeyUsage(bssl::der::Input extension_data) {
  std::vector<bssl::der::Input> extended_key_usage;
  if (!bssl::ParseEKUExtension(extension_data, &extended_key_usage)) {
    return std::nullopt;
  }

  std::string rv;
  for (const auto& oid : extended_key_usage) {
    std::string numeric_oid = OidToNumericString(oid);
    std::optional<std::string> oid_text = GetOidText(oid);

    // If oid is one that is recognized, display the text description along
    // with the numeric_oid. If we don't recognize the OID just display the
    // numeric OID alone.
    if (!oid_text) {
      rv += numeric_oid;
    } else {
      rv += l10n_util::GetStringFUTF8(IDS_CERT_EXT_KEY_USAGE_FORMAT,
                                      base::UTF8ToUTF16(*oid_text),
                                      base::UTF8ToUTF16(numeric_oid));
    }
    rv += '\n';
  }
  return rv;
}

OptionalStringOrError ProcessNameValue(bssl::der::Input name_value) {
  bssl::RDNSequence rdns;
  if (!bssl::ParseNameValue(name_value, &rdns)) {
    return Error();
  }
  return RDNSequenceToStringMultiLine(rdns);
}

std::string FormatGeneralName(std::u16string key, std::string_view value) {
  return l10n_util::GetStringFUTF8(IDS_CERT_UNKNOWN_OID_INFO_FORMAT, key,
                                   base::UTF8ToUTF16(value)) +
         '\n';
}

std::string FormatGeneralName(int key_string_id, std::string_view value) {
  return FormatGeneralName(l10n_util::GetStringUTF16(key_string_id), value);
}

bool ParseOtherName(bssl::der::Input other_name,
                    bssl::der::Input* type,
                    bssl::der::Input* value) {
  // OtherName ::= SEQUENCE {
  //      type-id    OBJECT IDENTIFIER,
  //      value      [0] EXPLICIT ANY DEFINED BY type-id }
  bssl::der::Parser sequence_parser(other_name);
  return sequence_parser.ReadTag(CBS_ASN1_OBJECT, type) &&
         sequence_parser.ReadTag(
             CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0, value) &&
         !sequence_parser.HasMore();
}

std::optional<std::string> ProcessGeneralNames(
    const bssl::GeneralNames& names) {
  // Note: The old x509_certificate_model_nss impl would process names in the
  // order they appeared in the certificate, whereas this impl parses names
  // into different lists by each type and then processes those in order.
  // Probably doesn't matter.
  std::string rv;
  for (const auto& other_name : names.other_names) {
    bssl::der::Input type;
    bssl::der::Input value;
    if (!ParseOtherName(other_name, &type, &value)) {
      return std::nullopt;
    }
    // x509_certificate_model_nss went a bit further in parsing certain
    // otherName types, but it probably isn't worth bothering.
    rv += FormatGeneralName(base::UTF8ToUTF16(GetOidTextOrNumeric(type)),
                            ProcessRawBytes(value));
  }
  for (const auto& rfc822_name : names.rfc822_names) {
    // TODO(mattm): do something about newlines (or other control chars)?
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_RFC822_NAME, rfc822_name);
  }
  for (const auto& dns_name : names.dns_names) {
    // TODO(mattm): Should probably do ProcessIDN on dnsNames from
    // subjectAltName like we do on subject commonName?
    // TODO(mattm): do something about newlines (or other control chars)?
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_DNS_NAME, dns_name);
  }
  for (const auto& x400_address : names.x400_addresses) {
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_X400_ADDRESS,
                            ProcessRawBytes(x400_address));
  }
  for (const auto& directory_name : names.directory_names) {
    OptionalStringOrError name = ProcessNameValue(directory_name);
    if (!absl::holds_alternative<std::string>(name))
      return std::nullopt;
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_DIRECTORY_NAME,
                            absl::get<std::string>(name));
  }
  for (const auto& edi_party_name : names.edi_party_names) {
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_EDI_PARTY_NAME,
                            ProcessRawBytes(edi_party_name));
  }
  for (const auto& uniform_resource_identifier :
       names.uniform_resource_identifiers) {
    // TODO(mattm): do something about newlines (or other control chars)?
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_URI,
                            uniform_resource_identifier);
  }
  for (const auto& ip_address_bytes : names.ip_addresses) {
    net::IPAddress ip_address(ip_address_bytes);
    // The `GeneralNames` parser should guarantee this.
    CHECK(ip_address.IsValid());
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_IP_ADDRESS,
                            ip_address.ToString());
  }
  for (const auto& ip_address_range : names.ip_address_ranges) {
    net::IPAddress ip_address(ip_address_range.first);
    net::IPAddress mask(ip_address_range.second);
    // The `GeneralNames` parser should guarantee this.
    CHECK(ip_address.IsValid());
    CHECK(mask.IsValid());
    rv += FormatGeneralName(
        IDS_CERT_GENERAL_NAME_IP_ADDRESS,
        ip_address.ToString() + '/' +
            base::NumberToString(net::MaskPrefixLength(mask)));
  }
  for (const auto& registered_id : names.registered_ids) {
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_REGISTERED_ID,
                            GetOidTextOrNumeric(registered_id));
  }

  return rv;
}

std::optional<std::string> ProcessGeneralNamesTlv(
    bssl::der::Input extension_data) {
  bssl::CertErrors unused_errors;
  std::unique_ptr<bssl::GeneralNames> alt_names =
      bssl::GeneralNames::Create(extension_data, &unused_errors);
  if (!alt_names)
    return std::nullopt;
  return ProcessGeneralNames(*alt_names);
}

std::optional<std::string> ProcessGeneralNamesValue(
    bssl::der::Input general_names_value) {
  bssl::CertErrors unused_errors;
  std::unique_ptr<bssl::GeneralNames> alt_names =
      bssl::GeneralNames::CreateFromValue(general_names_value, &unused_errors);
  if (!alt_names)
    return std::nullopt;
  return ProcessGeneralNames(*alt_names);
}

std::optional<std::string> ProcessSubjectKeyId(
    bssl::der::Input extension_data) {
  bssl::der::Input subject_key_identifier;
  if (!bssl::ParseSubjectKeyIdentifier(extension_data,
                                       &subject_key_identifier)) {
    return std::nullopt;
  }
  return l10n_util::GetStringFUTF8(
      IDS_CERT_KEYID_FORMAT,
      base::ASCIIToUTF16(ProcessRawBytes(subject_key_identifier)));
}

std::optional<std::string> ProcessAuthorityKeyId(
    bssl::der::Input extension_data) {
  bssl::ParsedAuthorityKeyIdentifier authority_key_id;
  if (!bssl::ParseAuthorityKeyIdentifier(extension_data, &authority_key_id)) {
    return std::nullopt;
  }

  std::string rv;
  if (authority_key_id.key_identifier) {
    rv += l10n_util::GetStringFUTF8(
        IDS_CERT_KEYID_FORMAT,
        base::ASCIIToUTF16(ProcessRawBytes(*authority_key_id.key_identifier)));
    rv += '\n';
  }
  if (authority_key_id.authority_cert_issuer) {
    std::optional<std::string> s =
        ProcessGeneralNamesValue(*authority_key_id.authority_cert_issuer);
    if (!s)
      return std::nullopt;
    rv += l10n_util::GetStringFUTF8(IDS_CERT_ISSUER_FORMAT,
                                    base::UTF8ToUTF16(*s));
    rv += '\n';
  }
  if (authority_key_id.authority_cert_serial_number) {
    rv += l10n_util::GetStringFUTF8(
        IDS_CERT_SERIAL_NUMBER_FORMAT,
        base::ASCIIToUTF16(
            ProcessRawBytes(*authority_key_id.authority_cert_serial_number)));
    rv += '\n';
  }

  return rv;
}

std::optional<std::string> ProcessUserNoticeDisplayText(
    CBS_ASN1_TAG tag,
    bssl::der::Input value) {
  std::string display_text;
  switch (tag) {
    case CBS_ASN1_IA5STRING:
      if (!bssl::der::ParseIA5String(value, &display_text)) {
        return std::nullopt;
      }
      break;
    case CBS_ASN1_VISIBLESTRING:
      if (!bssl::der::ParseVisibleString(value, &display_text)) {
        return std::nullopt;
      }
      break;
    case CBS_ASN1_BMPSTRING:
      if (!bssl::der::ParseBmpString(value, &display_text)) {
        return std::nullopt;
      }
      break;
    case CBS_ASN1_UTF8STRING:
      if (!base::IsStringUTF8AllowingNoncharacters(value.AsStringView())) {
        return std::nullopt;
      }
      display_text = value.AsString();
      break;
    default:
      return std::nullopt;
  }
  // TODO(mattm): do something about newlines (or other control chars)?
  return display_text;
}

std::optional<std::string> ProcessUserNotice(bssl::der::Input qualifier) {
  // RFC 5280 section 4.2.1.4:
  //
  //    UserNotice ::= SEQUENCE {
  //         noticeRef        NoticeReference OPTIONAL,
  //         explicitText     DisplayText OPTIONAL }
  //
  //    NoticeReference ::= SEQUENCE {
  //         organization     DisplayText,
  //         noticeNumbers    SEQUENCE OF INTEGER }
  //
  //    DisplayText ::= CHOICE {
  //         ia5String        IA5String      (SIZE (1..200)),
  //         visibleString    VisibleString  (SIZE (1..200)),
  //         bmpString        BMPString      (SIZE (1..200)),
  //         utf8String       UTF8String     (SIZE (1..200)) }

  bssl::der::Parser outer_parser(qualifier);
  bssl::der::Parser parser;
  if (!outer_parser.ReadSequence(&parser) || outer_parser.HasMore())
    return std::nullopt;

  std::optional<bssl::der::Input> notice_ref_value;
  if (!parser.ReadOptionalTag(CBS_ASN1_SEQUENCE, &notice_ref_value)) {
    return std::nullopt;
  }

  std::string rv;
  if (notice_ref_value) {
    bssl::der::Parser notice_ref_parser(*notice_ref_value);
    CBS_ASN1_TAG organization_tag;
    bssl::der::Input organization_value;
    if (!notice_ref_parser.ReadTagAndValue(&organization_tag,
                                           &organization_value)) {
      return std::nullopt;
    }
    std::optional<std::string> s =
        ProcessUserNoticeDisplayText(organization_tag, organization_value);
    if (!s)
      return std::nullopt;
    rv += *s;
    rv += " - ";

    bssl::der::Parser notice_numbers_parser;
    if (!notice_ref_parser.ReadSequence(&notice_numbers_parser))
      return std::nullopt;
    bool first = true;
    while (notice_numbers_parser.HasMore()) {
      bssl::der::Input notice_number;
      if (!notice_numbers_parser.ReadTag(CBS_ASN1_INTEGER, &notice_number)) {
        return std::nullopt;
      }
      if (!first)
        rv += ", ";
      rv += '#';
      uint64_t number;
      if (bssl::der::ParseUint64(notice_number, &number)) {
        rv += base::NumberToString(number);
      } else {
        rv += ProcessRawBytes(notice_number);
      }
      first = false;
    }
  }

  if (parser.HasMore()) {
    CBS_ASN1_TAG explicit_text_tag;
    bssl::der::Input explicit_text_value;
    if (!parser.ReadTagAndValue(&explicit_text_tag, &explicit_text_value))
      return std::nullopt;
    rv += "\n    ";
    std::optional<std::string> s =
        ProcessUserNoticeDisplayText(explicit_text_tag, explicit_text_value);
    if (!s)
      return std::nullopt;
    rv += *s;
  }

  if (parser.HasMore())
    return std::nullopt;

  return rv;
}

std::optional<std::string> ProcessCertificatePolicies(
    bssl::der::Input extension_data) {
  std::vector<bssl::PolicyInformation> policies;
  bssl::CertErrors errors;
  if (!bssl::ParseCertificatePoliciesExtension(extension_data, &policies,
                                               &errors)) {
    return std::nullopt;
  }
  std::string rv;
  for (const auto& policy_info : policies) {
    std::string key = GetOidTextOrNumeric(policy_info.policy_oid);
    // If there are policy qualifiers, display the oid text
    // with a ':', otherwise just put the oid text and a newline.
    if (policy_info.policy_qualifiers.empty()) {
      rv += key;
    } else {
      rv += l10n_util::GetStringFUTF8(IDS_CERT_MULTILINE_INFO_START_FORMAT,
                                      base::UTF8ToUTF16(key));
    }
    rv += '\n';

    if (!policy_info.policy_qualifiers.empty()) {
      for (const auto& qualifier_info : policy_info.policy_qualifiers) {
        rv += "  ";
        rv += l10n_util::GetStringFUTF8(IDS_CERT_MULTILINE_INFO_START_FORMAT,
                                        base::UTF8ToUTF16(GetOidTextOrNumeric(
                                            qualifier_info.qualifier_oid)));
        if (qualifier_info.qualifier_oid ==
            bssl::der::Input(bssl::kCpsPointerId)) {
          std::optional<std::string> s =
              ProcessIA5String(qualifier_info.qualifier);
          if (!s)
            return std::nullopt;
          rv += "    ";
          rv += *s;
        } else if (qualifier_info.qualifier_oid ==
                   bssl::der::Input(bssl::kUserNoticeId)) {
          std::optional<std::string> s =
              ProcessUserNotice(qualifier_info.qualifier);
          if (!s)
            return std::nullopt;
          rv += *s;
        } else {
          rv += ProcessRawBytes(qualifier_info.qualifier);
        }
        rv += '\n';
      }
    }
  }
  return rv;
}

std::optional<std::string> ProcessCrlDistributionPoints(
    bssl::der::Input extension_data) {
  std::vector<bssl::ParsedDistributionPoint> distribution_points;
  if (!ParseCrlDistributionPoints(extension_data, &distribution_points))
    return std::nullopt;

  //    ReasonFlags ::= BIT STRING {
  static const int kReasonStrings[] = {
      //         unused                  (0),
      IDS_CERT_REVOCATION_REASON_UNUSED,
      //         keyCompromise           (1),
      IDS_CERT_REVOCATION_REASON_KEY_COMPROMISE,
      //         cACompromise            (2),
      IDS_CERT_REVOCATION_REASON_CA_COMPROMISE,
      //         affiliationChanged      (3),
      IDS_CERT_REVOCATION_REASON_AFFILIATION_CHANGED,
      //         superseded              (4),
      IDS_CERT_REVOCATION_REASON_SUPERSEDED,
      //         cessationOfOperation    (5),
      IDS_CERT_REVOCATION_REASON_CESSATION_OF_OPERATION,
      //         certificateHold         (6),
      IDS_CERT_REVOCATION_REASON_CERTIFICATE_HOLD,
      // These aren't included as they would be challenging to translate and
      // are irrelevant for a web browser. (Actually all of these are
      // kinda irrelevant as we don't support CRL reasons.)
      //         privilegeWithdrawn      (7),
      //         aACompromise            (8) }
  };

  std::string rv;
  for (const auto& dp : distribution_points) {
    if (dp.distribution_point_fullname) {
      std::optional<std::string> s =
          ProcessGeneralNames(*dp.distribution_point_fullname);
      if (!s)
        return std::nullopt;
      rv += *s;
    }

    if (dp.distribution_point_name_relative_to_crl_issuer) {
      bssl::RelativeDistinguishedName name_relative_to_crl_issuer;
      bssl::der::Parser rdnParser(
          *dp.distribution_point_name_relative_to_crl_issuer);
      if (!bssl::ReadRdn(&rdnParser, &name_relative_to_crl_issuer)) {
        return std::nullopt;
      }
      std::string s = ProcessRDN(name_relative_to_crl_issuer);
      if (s.empty())
        return std::nullopt;
      rv += s;
    }

    if (dp.reasons) {
      std::optional<std::string> s =
          ProcessBitStringValue(*dp.reasons, kReasonStrings, ',');
      if (!s)
        return std::nullopt;
      rv += *s + '\n';
    }

    if (dp.crl_issuer) {
      bssl::CertErrors unused_errors;
      auto crl_issuer =
          bssl::GeneralNames::CreateFromValue(*dp.crl_issuer, &unused_errors);
      if (!crl_issuer)
        return std::nullopt;
      std::optional<std::string> s = ProcessGeneralNames(*crl_issuer);
      if (!s)
        return std::nullopt;
      rv += l10n_util::GetStringFUTF8(IDS_CERT_ISSUER_FORMAT,
                                      base::UTF8ToUTF16(*s));
    }
  }

  return rv;
}

std::optional<std::string> ProcessAuthorityInfoAccess(
    bssl::der::Input extension_data) {
  std::vector<bssl::AuthorityInfoAccessDescription> access_descriptions;
  if (!bssl::ParseAuthorityInfoAccess(extension_data, &access_descriptions)) {
    return std::nullopt;
  }

  std::string rv;
  for (const auto& access_description : access_descriptions) {
    bssl::GeneralNames name;
    bssl::CertErrors unused_errors;
    if (!bssl::ParseGeneralName(access_description.access_location,
                                bssl::GeneralNames::IP_ADDRESS_ONLY, &name,
                                &unused_errors)) {
      return std::nullopt;
    }

    std::optional<std::string> s = ProcessGeneralNames(name);
    if (!s)
      return std::nullopt;
    std::u16string location_str = base::UTF8ToUTF16(*s);
    if (access_description.access_method_oid ==
        bssl::der::Input(bssl::kAdOcspOid)) {
      rv += l10n_util::GetStringFUTF8(IDS_CERT_OCSP_RESPONDER_FORMAT,
                                      location_str);
    } else if (access_description.access_method_oid ==
               bssl::der::Input(bssl::kAdCaIssuersOid)) {
      rv += l10n_util::GetStringFUTF8(IDS_CERT_CA_ISSUERS_FORMAT, location_str);
    } else {
      rv += l10n_util::GetStringFUTF8(
          IDS_CERT_UNKNOWN_OID_INFO_FORMAT,
          base::UTF8ToUTF16(
              GetOidTextOrNumeric(access_description.access_method_oid)),
          location_str);
    }
  }

  return rv;
}

std::string ProcessAlgorithmIdentifier(bssl::der::Input algorithm_tlv) {
  bssl::der::Input oid;
  bssl::der::Input params;
  if (!bssl::ParseAlgorithmIdentifier(algorithm_tlv, &oid, &params)) {
    return std::string();
  }
  return GetOidTextOrNumeric(oid);
}

bool ParseSubjectPublicKeyInfo(bssl::der::Input spki_tlv,
                               bssl::der::Input* algorithm_tlv,
                               bssl::der::Input* subject_public_key_value) {
  bssl::der::Parser spki_parser(spki_tlv);

  //    SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //         algorithm            AlgorithmIdentifier,
  //         subjectPublicKey     BIT STRING  }
  bssl::der::Parser sequence_parser;
  if (!spki_parser.ReadSequence(&sequence_parser))
    return false;

  if (!sequence_parser.ReadRawTLV(algorithm_tlv))
    return false;

  if (!sequence_parser.ReadTag(CBS_ASN1_BITSTRING, subject_public_key_value)) {
    return false;
  }

  if (sequence_parser.HasMore())
    return false;

  return true;
}

std::vector<uint8_t> BIGNUMBytes(const BIGNUM* bn) {
  std::vector<uint8_t> ret(BN_num_bytes(bn));
  BN_bn2bin(bn, ret.data());
  return ret;
}

}  // namespace

X509CertificateModel::X509CertificateModel(
    bssl::UniquePtr<CRYPTO_BUFFER> cert_data,
    std::string nickname)
    : nickname_(std::move(nickname)), cert_data_(std::move(cert_data)) {
  DCHECK(cert_data_);

  bssl::ParseCertificateOptions options;
  options.allow_invalid_serial_numbers = true;
  bssl::CertErrors unused_errors;
  if (!bssl::ParseCertificate(
          bssl::der::Input(
              net::x509_util::CryptoBufferAsSpan(cert_data_.get())),
          &tbs_certificate_tlv_, &signature_algorithm_tlv_, &signature_value_,
          &unused_errors) ||
      !ParseTbsCertificate(tbs_certificate_tlv_, options, &tbs_,
                           &unused_errors) ||
      !bssl::ParseName(tbs_.subject_tlv, &subject_rdns_) ||
      !bssl::ParseName(tbs_.issuer_tlv, &issuer_rdns_)) {
    return;
  }
  if (tbs_.extensions_tlv && !ParseExtensions(tbs_.extensions_tlv.value())) {
    return;
  }
  parsed_successfully_ = true;
}

X509CertificateModel::X509CertificateModel(X509CertificateModel&& other) =
    default;

X509CertificateModel::~X509CertificateModel() = default;

std::string X509CertificateModel::HashCertSHA256() const {
  auto hash =
      crypto::SHA256Hash(net::x509_util::CryptoBufferAsSpan(cert_data_.get()));
  return base::ToLowerASCII(base::HexEncode(hash));
}

std::string X509CertificateModel::GetTitle() const {
  if (!nickname_.empty())
    return nickname_;

  if (!parsed_successfully_)
    return HashCertSHA256();

  if (!subject_rdns_.empty()) {
    OptionalStringOrError common_name = FindLastNameOfType(
        bssl::der::Input(bssl::kTypeCommonNameOid), subject_rdns_);
    if (auto* str = absl::get_if<std::string>(&common_name); str)
      return std::move(*str);
    if (absl::holds_alternative<Error>(common_name))
      return HashCertSHA256();

    std::string rv;
    if (!bssl::ConvertToRFC2253(subject_rdns_, &rv)) {
      return HashCertSHA256();
    }
    return rv;
  }

  if (subject_alt_names_) {
    // TODO(mattm): do something about newlines (or other control chars)?
    if (!subject_alt_names_->dns_names.empty())
      return std::string(subject_alt_names_->dns_names[0]);
    if (!subject_alt_names_->rfc822_names.empty())
      return std::string(subject_alt_names_->rfc822_names[0]);
  }

  return HashCertSHA256();
}

std::string X509CertificateModel::GetVersion() const {
  DCHECK(parsed_successfully_);
  switch (tbs_.version) {
    case bssl::CertificateVersion::V1:
      return "1";
    case bssl::CertificateVersion::V2:
      return "2";
    case bssl::CertificateVersion::V3:
      return "3";
  }
}

std::string X509CertificateModel::GetSerialNumberHexified() const {
  DCHECK(parsed_successfully_);
  return ProcessRawBytesWithSeparators(tbs_.serial_number, ':', ':');
}

bool X509CertificateModel::GetTimes(base::Time* not_before,
                                    base::Time* not_after) const {
  DCHECK(parsed_successfully_);
  return net::GeneralizedTimeToTime(tbs_.validity_not_before, not_before) &&
         net::GeneralizedTimeToTime(tbs_.validity_not_after, not_after);
}

OptionalStringOrError X509CertificateModel::GetIssuerCommonName() const {
  DCHECK(parsed_successfully_);
  // Return the last (most specific) commonName. This matches NSS
  // CERT_GetCommonName.
  return FindLastNameOfType(bssl::der::Input(bssl::kTypeCommonNameOid),
                            issuer_rdns_);
}

OptionalStringOrError X509CertificateModel::GetIssuerOrgName() const {
  DCHECK(parsed_successfully_);
  // Return the first (most general) orgName. This matches NSS CERT_GetOrgName.
  return FindFirstNameOfType(bssl::der::Input(bssl::kTypeOrganizationNameOid),
                             issuer_rdns_);
}

OptionalStringOrError X509CertificateModel::GetIssuerOrgUnitName() const {
  DCHECK(parsed_successfully_);
  // Return the first (most general) orgUnitName. This matches NSS
  // CERT_GetOrgUnitName.
  return FindFirstNameOfType(
      bssl::der::Input(bssl::kTypeOrganizationUnitNameOid), issuer_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectCommonName() const {
  DCHECK(parsed_successfully_);
  // Return the last (most specific) commonName. This matches NSS
  // CERT_GetCommonName.
  return FindLastNameOfType(bssl::der::Input(bssl::kTypeCommonNameOid),
                            subject_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectOrgName() const {
  DCHECK(parsed_successfully_);
  // Return the first (most general) orgName. This matches NSS CERT_GetOrgName.
  return FindFirstNameOfType(bssl::der::Input(bssl::kTypeOrganizationNameOid),
                             subject_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectOrgUnitName() const {
  DCHECK(parsed_successfully_);
  // Return the first (most general) orgUnitName. This matches NSS
  // CERT_GetOrgUnitName.
  return FindFirstNameOfType(
      bssl::der::Input(bssl::kTypeOrganizationUnitNameOid), subject_rdns_);
}

OptionalStringOrError X509CertificateModel::GetIssuerName() const {
  DCHECK(parsed_successfully_);
  return RDNSequenceToStringMultiLine(issuer_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectName() const {
  DCHECK(parsed_successfully_);
  return RDNSequenceToStringMultiLine(subject_rdns_);
}

std::vector<Extension> X509CertificateModel::GetExtensions(
    std::string_view critical_label,
    std::string_view non_critical_label) const {
  DCHECK(parsed_successfully_);
  std::vector<Extension> extensions;
  for (const auto& extension : extensions_) {
    Extension processed_extension;
    processed_extension.name = GetOidTextOrNumeric(extension.oid);
    processed_extension.value =
        ProcessExtension(critical_label, non_critical_label, extension);
    extensions.push_back(processed_extension);
  }
  return extensions;
}

bool X509CertificateModel::ParseExtensions(
    const bssl::der::Input& extensions_tlv) {
  bssl::CertErrors unused_errors;
  bssl::der::Parser parser(extensions_tlv);

  //    Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
  bssl::der::Parser extensions_parser;
  if (!parser.ReadSequence(&extensions_parser))
    return false;

  // The Extensions SEQUENCE must contains at least 1 element (otherwise it
  // should have been omitted).
  if (!extensions_parser.HasMore())
    return false;

  while (extensions_parser.HasMore()) {
    bssl::ParsedExtension extension;

    bssl::der::Input extension_tlv;
    if (!extensions_parser.ReadRawTLV(&extension_tlv))
      return false;

    if (!ParseExtension(extension_tlv, &extension))
      return false;

    extensions_.push_back(extension);

    if (extension.oid == bssl::der::Input(bssl::kSubjectAltNameOid)) {
      subject_alt_names_ =
          bssl::GeneralNames::Create(extension.value, &unused_errors);
      if (!subject_alt_names_)
        return false;
    }
  }

  // By definition the input was a single Extensions sequence, so there
  // shouldn't be unconsumed data.
  if (parser.HasMore())
    return false;

  return true;
}

std::string X509CertificateModel::ProcessExtension(
    std::string_view critical_label,
    std::string_view non_critical_label,
    const bssl::ParsedExtension& extension) const {
  std::string_view criticality =
      extension.critical ? critical_label : non_critical_label;
  std::optional<std::string> processed_extension =
      ProcessExtensionData(extension);
  return base::StrCat(
      {criticality, "\n",
       (processed_extension
            ? *processed_extension
            : l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR))});
}

std::optional<std::string> X509CertificateModel::ProcessExtensionData(
    const bssl::ParsedExtension& extension) const {
  if (extension.oid == bssl::der::Input(kNetscapeCertificateTypeOid)) {
    return ProcessNSCertTypeExtension(extension.value);
  }
  if (extension.oid == bssl::der::Input(bssl::kKeyUsageOid)) {
    return ProcessKeyUsageExtension(extension.value);
  }
  if (extension.oid == bssl::der::Input(bssl::kBasicConstraintsOid)) {
    return ProcessBasicConstraints(extension.value);
  }
  if (extension.oid == bssl::der::Input(bssl::kExtKeyUsageOid)) {
    return ProcessExtKeyUsage(extension.value);
  }
  if (extension.oid == bssl::der::Input(bssl::kSubjectAltNameOid)) {
    // The subjectAltName extension was already parsed in the constructor, use
    // that rather than parse it again.
    DCHECK(subject_alt_names_);
    return ProcessGeneralNames(*subject_alt_names_);
  }
  if (extension.oid == bssl::der::Input(kIssuerAltNameOid)) {
    return ProcessGeneralNamesTlv(extension.value);
  }
  if (extension.oid == bssl::der::Input(bssl::kSubjectKeyIdentifierOid)) {
    return ProcessSubjectKeyId(extension.value);
  }
  if (extension.oid == bssl::der::Input(bssl::kAuthorityKeyIdentifierOid)) {
    return ProcessAuthorityKeyId(extension.value);
  }
  if (extension.oid == bssl::der::Input(bssl::kCertificatePoliciesOid)) {
    return ProcessCertificatePolicies(extension.value);
  }
  if (extension.oid == bssl::der::Input(bssl::kCrlDistributionPointsOid)) {
    return ProcessCrlDistributionPoints(extension.value);
  }
  if (extension.oid == bssl::der::Input(bssl::kAuthorityInfoAccessOid)) {
    return ProcessAuthorityInfoAccess(extension.value);
  }
  if (extension.oid == bssl::der::Input(kNetscapeBaseURLOid) ||
      extension.oid == bssl::der::Input(kNetscapeRevocationURLOid) ||
      extension.oid == bssl::der::Input(kNetscapeCARevocationURLOid) ||
      extension.oid == bssl::der::Input(kNetscapeCACertURLOid) ||
      extension.oid == bssl::der::Input(kNetscapeRenewalURLOid) ||
      extension.oid == bssl::der::Input(kNetscapeCAPolicyURLOid) ||
      extension.oid == bssl::der::Input(kNetscapeSSLServerNameOid) ||
      extension.oid == bssl::der::Input(kNetscapeCommentOid) ||
      extension.oid == bssl::der::Input(kNetscapeLostPasswordURLOid)) {
    return ProcessIA5String(extension.value);
  }
  // TODO(crbug.com/41395047): SCT
  // TODO(mattm): name constraints
  // TODO(mattm): policy mappings
  // TODO(mattm): policy constraints
  return ProcessRawBytes(extension.value);
}

std::string X509CertificateModel::ProcessSecAlgorithmSignature() const {
  DCHECK(parsed_successfully_);
  return ProcessAlgorithmIdentifier(signature_algorithm_tlv_);
}

std::string X509CertificateModel::ProcessSecAlgorithmSubjectPublicKey() const {
  DCHECK(parsed_successfully_);

  bssl::der::Input algorithm_tlv;
  bssl::der::Input unused_spk_value;
  if (!ParseSubjectPublicKeyInfo(tbs_.spki_tlv, &algorithm_tlv,
                                 &unused_spk_value)) {
    return std::string();
  }

  return ProcessAlgorithmIdentifier(algorithm_tlv);
}

std::string X509CertificateModel::ProcessSecAlgorithmSignatureWrap() const {
  DCHECK(parsed_successfully_);
  return ProcessAlgorithmIdentifier(tbs_.signature_algorithm_tlv);
}

std::string X509CertificateModel::ProcessSubjectPublicKeyInfo() const {
  DCHECK(parsed_successfully_);
  std::string rv = ProcessRawSubjectPublicKeyInfo(tbs_.spki_tlv);
  if (rv.empty())
    return std::string();
  return rv;
}

std::string X509CertificateModel::HashSpkiSHA256() const {
  DCHECK(parsed_successfully_);
  auto hash = crypto::SHA256Hash(tbs_.spki_tlv);
  return base::ToLowerASCII(base::HexEncode(hash));
}

std::string X509CertificateModel::ProcessRawBitsSignatureWrap() const {
  DCHECK(parsed_successfully_);
  return ProcessRawBytes(signature_value_.bytes());
}

// TODO(crbug.com/41453265): move to anonymous namespace once
// x509_certificate_model_nss is removed.
std::string ProcessIDN(const std::string& input) {
  if (!base::IsStringASCII(input))
    return input;

  // Convert the ASCII input to a string16 for ICU.
  std::u16string input16;
  input16.reserve(input.length());
  input16.insert(input16.end(), input.begin(), input.end());

  std::u16string output16 = url_formatter::IDNToUnicode(input);
  if (input16 == output16)
    return input;  // Input did not contain any encoded data.

  // Input contained encoded data, return formatted string showing original and
  // decoded forms.
  return l10n_util::GetStringFUTF8(IDS_CERT_INFO_IDN_VALUE_FORMAT, input16,
                                   output16);
}

std::string ProcessRawSubjectPublicKeyInfo(base::span<const uint8_t> spki_der) {
  bssl::UniquePtr<EVP_PKEY> public_key;
  if (!bssl::ParsePublicKey(bssl::der::Input(spki_der), &public_key)) {
    return std::string();
  }
  switch (EVP_PKEY_id(public_key.get())) {
    case EVP_PKEY_RSA: {
      RSA* rsa = EVP_PKEY_get0_RSA(public_key.get());
      // EVP_PKEY_get0_RSA can only fail if the type was wrong, which was just
      // checked in the switch.
      DCHECK(rsa);
      const BIGNUM* modulus = RSA_get0_n(rsa);
      const BIGNUM* public_exponent = RSA_get0_e(rsa);
      DCHECK(modulus);
      DCHECK(public_exponent);

      return l10n_util::GetStringFUTF8(
          IDS_CERT_RSA_PUBLIC_KEY_DUMP_FORMAT,
          base::NumberToString16(BN_num_bits(modulus)),
          base::UTF8ToUTF16(ProcessRawBytes(BIGNUMBytes(modulus))),
          base::NumberToString16(BN_num_bits(public_exponent)),
          base::UTF8ToUTF16(ProcessRawBytes(BIGNUMBytes(public_exponent))));
    }
      // TODO(mattm): handle other key types? (eg EVP_PKEY_EC)
  }

  bssl::der::Input unused_algorithm_tlv;
  bssl::der::Input subject_public_key_value;
  if (!ParseSubjectPublicKeyInfo(bssl::der::Input(spki_der),
                                 &unused_algorithm_tlv,
                                 &subject_public_key_value)) {
    return std::string();
  }
  return ProcessRawBytes(subject_public_key_value);
}

}  // namespace x509_certificate_model
