// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CERTIFICATE_MODEL_X509_CERTIFICATE_CONSTANTS_H_
#define COMPONENTS_CERTIFICATE_MODEL_X509_CERTIFICATE_CONSTANTS_H_

#include <cstdint>

#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/boringssl/src/pki/parse_name.h"

namespace x509_certificate_model {

// 2.5.4.46 NAME 'dnQualifier'
inline constexpr uint8_t kTypeDnQualifierOid[] = {0x55, 0x04, 0x2e};
// 2.5.4.15 NAME 'businessCategory'
inline constexpr uint8_t kTypeBusinessCategory[] = {0x55, 0x04, 0x0f};
// 2.5.4.17 NAME 'postalCode'
inline constexpr uint8_t kTypePostalCode[] = {0x55, 0x04, 0x11};

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
inline constexpr uint8_t kRFC1274UidOid[] = {0x09, 0x92, 0x26, 0x89, 0x93,
                                             0xf2, 0x2c, 0x64, 0x01, 0x01};
// rfc822Mailbox :: = {pilotAttributeType 3}
inline constexpr uint8_t kRFC1274MailOid[] = {0x09, 0x92, 0x26, 0x89, 0x93,
                                              0xf2, 0x2c, 0x64, 0x01, 0x03};

// From RFC 5280:
//     id-ce-subjectDirectoryAttributes OBJECT IDENTIFIER ::=  { id-ce 9 }
// In dotted notation: 2.5.29.9
inline constexpr uint8_t kSubjectDirectoryAttributesOid[] = {0x55, 0x1d, 0x09};
// From RFC 5280:
//     id-ce-issuerAltName OBJECT IDENTIFIER ::=  { id-ce 18 }
// In dotted notation: 2.5.29.18
inline constexpr uint8_t kIssuerAltNameOid[] = {0x55, 0x1d, 0x12};

// jurisdictionLocalityName (OID: 1.3.6.1.4.1.311.60.2.1.1)
inline constexpr uint8_t kEVJurisdictionLocalityName[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x3c, 0x02, 0x01, 0x01};
// jurisdictionStateOrProvinceName (OID: 1.3.6.1.4.1.311.60.2.1.2)
inline constexpr uint8_t kEVJurisdictionStateOrProvinceName[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x3c, 0x02, 0x01, 0x02};
// jurisdictionCountryName (OID: 1.3.6.1.4.1.311.60.2.1.3)
inline constexpr uint8_t kEVJurisdictionCountryName[] = {
    0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0x37, 0x3c, 0x02, 0x01, 0x03};

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

#endif  // COMPONENTS_CERTIFICATE_MODEL_X509_CERTIFICATE_CONSTANTS_H_
