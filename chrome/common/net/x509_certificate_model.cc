// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/hash/sha1.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "crypto/sha2.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/extended_key_usage.h"
#include "net/cert/internal/parse_name.h"
#include "net/cert/x509_util.h"
#include "net/der/encode_values.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/tag.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
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

// Old Netscape OIDs. Do we still need all these?
// #define NETSCAPE_OID 0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x42
// #define NETSCAPE_CERT_EXT NETSCAPE_OID, 0x01
//
// CONST_OID nsExtCertType[] = { NETSCAPE_CERT_EXT, 0x01 };
constexpr uint8_t kNetscapeCertificateTypeOid[] = {0x60, 0x86, 0x48, 0x01, 0x86,
                                                   0xf8, 0x42, 0x01, 0x01};

// The certificate viewer may be used to view client certificates, so use the
// relaxed parsing mode. See crbug.com/770323 and crbug.com/788655.
constexpr auto kNameStringHandling =
    net::X509NameAttribute::PrintableStringHandling::kAsUTF8Hack;

std::string ProcessRawBytes(net::der::Input data) {
  return x509_certificate_model::ProcessRawBytes(data.UnsafeData(),
                                                 data.Length());
}

OptionalStringOrError FindAttributeOfType(
    net::der::Input oid,
    const net::RelativeDistinguishedName& rdn) {
  // In X.509, RelativeDistinguishedName is a Set, so order has no meaning, and
  // generally only has one element anyway. Just traverse in encoded order.
  for (const net::X509NameAttribute& name_attribute : rdn) {
    if (name_attribute.type == oid) {
      std::string rv;
      if (!name_attribute.ValueAsStringWithUnsafeOptions(kNameStringHandling,
                                                         &rv)) {
        return Error();
      }
      return rv;
    }
  }
  return NotPresent();
}

// Returns the value of the most general name of |oid| type.
// Distinguished Names are specified in least to most specific.
OptionalStringOrError FindFirstNameOfType(net::der::Input oid,
                                          const net::RDNSequence& rdns) {
  for (const net::RelativeDistinguishedName& rdn : rdns) {
    OptionalStringOrError r = FindAttributeOfType(oid, rdn);
    if (!absl::holds_alternative<NotPresent>(r))
      return r;
  }
  return NotPresent();
}

// Returns the value of the most specific name of |oid| type.
// Distinguished Names are specified in least to most specific.
OptionalStringOrError FindLastNameOfType(net::der::Input oid,
                                         const net::RDNSequence& rdns) {
  for (const net::RelativeDistinguishedName& rdn : base::Reversed(rdns)) {
    OptionalStringOrError r = FindAttributeOfType(oid, rdn);
    if (!absl::holds_alternative<NotPresent>(r))
      return r;
  }
  return NotPresent();
}

// Returns a string containing the dotted numeric form of |oid| prefixed by
// "OID.", or an empty string on error.
std::string OidToNumericString(net::der::Input oid) {
  CBS cbs;
  CBS_init(&cbs, oid.UnsafeData(), oid.Length());
  bssl::UniquePtr<char> text(CBS_asn1_oid_to_text(&cbs));
  if (!text)
    return std::string();
  return std::string("OID.") + text.get();
}

constexpr auto kOidStringMap = base::MakeFixedFlatMap<net::der::Input, int>({
    // Distinguished Name fields:
    {net::der::Input(net::kTypeCommonNameOid), IDS_CERT_OID_AVA_COMMON_NAME},
    {net::der::Input(net::kTypeStateOrProvinceNameOid),
     IDS_CERT_OID_AVA_STATE_OR_PROVINCE},
    {net::der::Input(net::kTypeOrganizationNameOid),
     IDS_CERT_OID_AVA_ORGANIZATION_NAME},
    {net::der::Input(net::kTypeOrganizationUnitNameOid),
     IDS_CERT_OID_AVA_ORGANIZATIONAL_UNIT_NAME},
    {net::der::Input(kTypeDnQualifierOid), IDS_CERT_OID_AVA_DN_QUALIFIER},
    {net::der::Input(net::kTypeCountryNameOid), IDS_CERT_OID_AVA_COUNTRY_NAME},
    {net::der::Input(net::kTypeSerialNumberOid),
     IDS_CERT_OID_AVA_SERIAL_NUMBER},
    {net::der::Input(net::kTypeLocalityNameOid), IDS_CERT_OID_AVA_LOCALITY},
    {net::der::Input(net::kTypeDomainComponentOid), IDS_CERT_OID_AVA_DC},
    {net::der::Input(kRFC1274MailOid), IDS_CERT_OID_RFC1274_MAIL},
    {net::der::Input(kRFC1274UidOid), IDS_CERT_OID_RFC1274_UID},
    {net::der::Input(net::kTypeEmailAddressOid),
     IDS_CERT_OID_PKCS9_EMAIL_ADDRESS},

    // Extended Validation (EV) name fields:
    {net::der::Input(kTypeBusinessCategory), IDS_CERT_OID_BUSINESS_CATEGORY},
    {net::der::Input(kEVJurisdictionLocalityName),
     IDS_CERT_OID_EV_INCORPORATION_LOCALITY},
    {net::der::Input(kEVJurisdictionStateOrProvinceName),
     IDS_CERT_OID_EV_INCORPORATION_STATE},
    {net::der::Input(kEVJurisdictionCountryName),
     IDS_CERT_OID_EV_INCORPORATION_COUNTRY},
    {net::der::Input(net::kTypeStreetAddressOid),
     IDS_CERT_OID_AVA_STREET_ADDRESS},
    {net::der::Input(kTypePostalCode), IDS_CERT_OID_AVA_POSTAL_CODE},

    // Extension fields (including details of extensions):
    {net::der::Input(kNetscapeCertificateTypeOid), IDS_CERT_EXT_NS_CERT_TYPE},
    {net::der::Input(kSubjectDirectoryAttributesOid),
     IDS_CERT_X509_SUBJECT_DIRECTORY_ATTR},
    {net::der::Input(net::kSubjectKeyIdentifierOid),
     IDS_CERT_X509_SUBJECT_KEYID},
    {net::der::Input(net::kAuthorityKeyIdentifierOid),
     IDS_CERT_X509_AUTH_KEYID},
    {net::der::Input(net::kKeyUsageOid), IDS_CERT_X509_KEY_USAGE},
    {net::der::Input(net::kSubjectAltNameOid), IDS_CERT_X509_SUBJECT_ALT_NAME},
    {net::der::Input(kIssuerAltNameOid), IDS_CERT_X509_ISSUER_ALT_NAME},
    {net::der::Input(net::kBasicConstraintsOid),
     IDS_CERT_X509_BASIC_CONSTRAINTS},
    {net::der::Input(net::kNameConstraintsOid), IDS_CERT_X509_NAME_CONSTRAINTS},
    {net::der::Input(net::kCrlDistributionPointsOid),
     IDS_CERT_X509_CRL_DIST_POINTS},
    {net::der::Input(net::kCertificatePoliciesOid),
     IDS_CERT_X509_CERT_POLICIES},
    {net::der::Input(net::kPolicyMappingsOid), IDS_CERT_X509_POLICY_MAPPINGS},
    {net::der::Input(net::kPolicyConstraintsOid),
     IDS_CERT_X509_POLICY_CONSTRAINTS},
    {net::der::Input(net::kExtKeyUsageOid), IDS_CERT_X509_EXT_KEY_USAGE},
    {net::der::Input(net::kAuthorityInfoAccessOid),
     IDS_CERT_X509_AUTH_INFO_ACCESS},
    {net::der::Input(net::kCpsPointerId), IDS_CERT_PKIX_CPS_POINTER_QUALIFIER},
    {net::der::Input(net::kUserNoticeId), IDS_CERT_PKIX_USER_NOTICE_QUALIFIER},

    // Extended Key Usages:
    {net::der::Input(net::kAnyEKU), IDS_CERT_EKU_ANY_EKU},
    {net::der::Input(net::kServerAuth),
     IDS_CERT_EKU_TLS_WEB_SERVER_AUTHENTICATION},
    {net::der::Input(net::kClientAuth),
     IDS_CERT_EKU_TLS_WEB_CLIENT_AUTHENTICATION},
    {net::der::Input(net::kCodeSigning), IDS_CERT_EKU_CODE_SIGNING},
    {net::der::Input(net::kEmailProtection), IDS_CERT_EKU_EMAIL_PROTECTION},
    {net::der::Input(net::kTimeStamping), IDS_CERT_EKU_TIME_STAMPING},
    {net::der::Input(net::kOCSPSigning), IDS_CERT_EKU_OCSP_SIGNING},
    {net::der::Input(net::kNetscapeServerGatedCrypto),
     IDS_CERT_EKU_NETSCAPE_INTERNATIONAL_STEP_UP},
});

absl::optional<std::string> GetOidText(net::der::Input oid) {
  // TODO(crbug.com/1311404): this should be "const auto i" since it's an
  // iterator, but fixed_flat_map iterators are raw pointers and the
  // chromium-style plugin complains.
  const auto* i = kOidStringMap.find(oid);
  if (i != kOidStringMap.end())
    return l10n_util::GetStringUTF8(i->second);
  return absl::nullopt;
}

std::string GetOidTextOrNumeric(net::der::Input oid) {
  absl::optional<std::string> oid_text = GetOidText(oid);
  return oid_text ? *oid_text : OidToNumericString(oid);
}

std::string ProcessRDN(const net::RelativeDistinguishedName& rdn) {
  std::string rv;
  // In X.509, RelativeDistinguishedName is a Set, so "last" has no meaning,
  // and generally only has one element anyway.  Just traverse in encoded
  // order.
  for (const net::X509NameAttribute& name_attribute : rdn) {
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
    if (name_attribute.type == net::der::Input(net::kTypeCommonNameOid))
      value = ProcessIDN(value);
    rv += value;
    rv += "\n";
  }
  return rv;
}

// Note: This was called ProcessName in the x509_certificate_model_nss impl.
OptionalStringOrError RDNSequenceToStringMultiLine(
    const net::RDNSequence& rdns) {
  if (rdns.empty())
    return NotPresent();

  std::string rv;
  // Note: this has high level similarity to net::ConvertToRFC2253, but
  // this one is multi-line, and prints in reverse order, and has a different
  // set of oids that it has printable names for, and different handling of
  // unprintable values, and IDN processing...
  for (const net::RelativeDistinguishedName& rdn : base::Reversed(rdns)) {
    std::string rdn_value = ProcessRDN(rdn);
    if (rdn_value.empty())
      return Error();
    rv += rdn_value;
  }
  return rv;
}

// Returns a comma-separated string of the strings in |string_map| for the bits
// in |bitfield| that are set.
// string_map may contain -1 for reserved positions that should not be set.
absl::optional<std::string> ProcessBitField(net::der::BitString bitfield,
                                            base::span<const int> string_map,
                                            char separator) {
  std::string rv;
  for (size_t i = 0; i < string_map.size(); ++i) {
    if (bitfield.AssertsBit(i)) {
      int string_id = string_map[i];
      // TODO(mattm): is returning an error here correct? Or should it encode
      // some generic string like "reserved bit N set"?
      if (string_id < 0)
        return absl::nullopt;
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
absl::optional<std::string> ProcessBitStringExtension(
    net::der::Input extension_data,
    base::span<const int> string_map,
    char separator) {
  net::der::Input value;
  net::der::Parser parser(extension_data);
  if (!parser.ReadTag(net::der::kBitString, &value) || parser.HasMore()) {
    return absl::nullopt;
  }
  absl::optional<net::der::BitString> decoded = net::der::ParseBitString(value);
  if (!decoded) {
    return absl::nullopt;
  }

  return ProcessBitField(decoded.value(), string_map, separator);
}

absl::optional<std::string> ProcessNSCertTypeExtension(
    net::der::Input extension_data) {
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

absl::optional<std::string> ProcessKeyUsageExtension(
    net::der::Input extension_data) {
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
  absl::optional<std::string> rv =
      ProcessBitStringExtension(extension_data, usage_strings, '\n');
  if (rv && rv->empty()) {
    // RFC 5280 4.2.1.3:
    // When the keyUsage extension appears in a certificate, at least one of
    // the bits MUST be set to 1.
    return absl::nullopt;
  }
  return rv;
}

absl::optional<std::string> ProcessBasicConstraints(
    net::der::Input extension_data) {
  net::ParsedBasicConstraints basic_constraints;
  if (!net::ParseBasicConstraints(extension_data, &basic_constraints))
    return absl::nullopt;

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

absl::optional<std::string> ProcessExtKeyUsage(net::der::Input extension_data) {
  std::vector<net::der::Input> extended_key_usage;
  if (!net::ParseEKUExtension(extension_data, &extended_key_usage))
    return absl::nullopt;

  std::string rv;
  for (const auto& oid : extended_key_usage) {
    std::string numeric_oid = OidToNumericString(oid);
    absl::optional<std::string> oid_text = GetOidText(oid);

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

OptionalStringOrError ProcessNameValue(net::der::Input name_value) {
  net::RDNSequence rdns;
  if (!net::ParseNameValue(name_value, &rdns))
    return Error();
  return RDNSequenceToStringMultiLine(rdns);
}

std::string FormatGeneralName(std::u16string key, base::StringPiece value) {
  return l10n_util::GetStringFUTF8(IDS_CERT_UNKNOWN_OID_INFO_FORMAT, key,
                                   base::UTF8ToUTF16(value)) +
         '\n';
}

std::string FormatGeneralName(int key_string_id, base::StringPiece value) {
  return FormatGeneralName(l10n_util::GetStringUTF16(key_string_id), value);
}

bool ParseOtherName(net::der::Input other_name,
                    net::der::Input* type,
                    net::der::Input* value) {
  // OtherName ::= SEQUENCE {
  //      type-id    OBJECT IDENTIFIER,
  //      value      [0] EXPLICIT ANY DEFINED BY type-id }
  net::der::Parser sequence_parser(other_name);
  return sequence_parser.ReadTag(net::der::kOid, type) &&
         sequence_parser.ReadTag(net::der::ContextSpecificConstructed(0),
                                 value) &&
         !sequence_parser.HasMore();
}

absl::optional<std::string> ProcessGeneralNames(
    const net::GeneralNames& names) {
  // Note: The old x509_certificate_model_nss impl would process names in the
  // order they appeared in the certificate, whereas this impl parses names
  // into different lists by each type and then processes those in order.
  // Probably doesn't matter.
  std::string rv;
  for (const auto& other_name : names.other_names) {
    net::der::Input type;
    net::der::Input value;
    if (!ParseOtherName(other_name, &type, &value)) {
      return absl::nullopt;
    }
    // x509_certificate_model_nss went a bit further in parsing certain
    // otherName types, but it probably isn't worth bothering.
    rv += FormatGeneralName(base::UTF8ToUTF16(GetOidTextOrNumeric(type)),
                            ProcessRawBytes(value));
  }
  for (const auto& rfc822_name : names.rfc822_names) {
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_RFC822_NAME, rfc822_name);
  }
  for (const auto& dns_name : names.dns_names) {
    // TODO(mattm): Should probably do ProcessIDN on dnsNames from
    // subjectAltName like we do on subject commonName?
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_DNS_NAME, dns_name);
  }
  for (const auto& x400_address : names.x400_addresses) {
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_X400_ADDRESS,
                            ProcessRawBytes(x400_address));
  }
  for (const auto& directory_name : names.directory_names) {
    OptionalStringOrError name = ProcessNameValue(directory_name);
    if (!absl::holds_alternative<std::string>(name))
      return absl::nullopt;
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_DIRECTORY_NAME,
                            absl::get<std::string>(name));
  }
  for (const auto& edi_party_name : names.edi_party_names) {
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_EDI_PARTY_NAME,
                            ProcessRawBytes(edi_party_name));
  }
  for (const auto& uniform_resource_identifier :
       names.uniform_resource_identifiers) {
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_URI,
                            uniform_resource_identifier);
  }
  for (const auto& ip_address : names.ip_addresses) {
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_IP_ADDRESS,
                            ip_address.ToString());
  }
  for (const auto& ip_address_range : names.ip_address_ranges) {
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_IP_ADDRESS,
                            ip_address_range.first.ToString() + '/' +
                                base::NumberToString(ip_address_range.second));
  }
  for (const auto& registered_id : names.registered_ids) {
    rv += FormatGeneralName(IDS_CERT_GENERAL_NAME_REGISTERED_ID,
                            GetOidTextOrNumeric(registered_id));
  }

  return rv;
}

absl::optional<std::string> ProcessGeneralNamesTlv(
    net::der::Input extension_data) {
  net::CertErrors unused_errors;
  std::unique_ptr<net::GeneralNames> alt_names =
      net::GeneralNames::Create(extension_data, &unused_errors);
  if (!alt_names)
    return absl::nullopt;
  return ProcessGeneralNames(*alt_names);
}

absl::optional<std::string> ProcessGeneralNamesValue(
    net::der::Input general_names_value) {
  net::CertErrors unused_errors;
  std::unique_ptr<net::GeneralNames> alt_names =
      net::GeneralNames::CreateFromValue(general_names_value, &unused_errors);
  if (!alt_names)
    return absl::nullopt;
  return ProcessGeneralNames(*alt_names);
}

absl::optional<std::string> ProcessSubjectKeyId(
    net::der::Input extension_data) {
  net::der::Input subject_key_identifier;
  if (!net::ParseSubjectKeyIdentifier(extension_data, &subject_key_identifier))
    return absl::nullopt;
  return l10n_util::GetStringFUTF8(
      IDS_CERT_KEYID_FORMAT,
      base::ASCIIToUTF16(ProcessRawBytes(subject_key_identifier)));
}

absl::optional<std::string> ProcessAuthorityKeyId(
    net::der::Input extension_data) {
  net::ParsedAuthorityKeyIdentifier authority_key_id;
  if (!net::ParseAuthorityKeyIdentifier(extension_data, &authority_key_id))
    return absl::nullopt;

  std::string rv;
  if (authority_key_id.key_identifier) {
    rv += l10n_util::GetStringFUTF8(
        IDS_CERT_KEYID_FORMAT,
        base::ASCIIToUTF16(ProcessRawBytes(*authority_key_id.key_identifier)));
    rv += '\n';
  }
  if (authority_key_id.authority_cert_issuer) {
    absl::optional<std::string> s =
        ProcessGeneralNamesValue(*authority_key_id.authority_cert_issuer);
    if (!s)
      return absl::nullopt;
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

}  // namespace

X509CertificateModel::X509CertificateModel(
    bssl::UniquePtr<CRYPTO_BUFFER> cert_data,
    std::string nickname)
    : nickname_(std::move(nickname)), cert_data_(std::move(cert_data)) {
  DCHECK(cert_data_);

  net::ParseCertificateOptions options;
  options.allow_invalid_serial_numbers = true;
  net::CertErrors unused_errors;
  if (!net::ParseCertificate(
          net::der::Input(CRYPTO_BUFFER_data(cert_data_.get()),
                          CRYPTO_BUFFER_len(cert_data_.get())),
          &tbs_certificate_tlv_, &signature_algorithm_tlv_, &signature_value_,
          &unused_errors) ||
      !ParseTbsCertificate(tbs_certificate_tlv_, options, &tbs_,
                           &unused_errors) ||
      !net::ParseName(tbs_.subject_tlv, &subject_rdns_) ||
      !net::ParseName(tbs_.issuer_tlv, &issuer_rdns_)) {
    return;
  }
  if (tbs_.extensions_tlv && !ParseExtensions(tbs_.extensions_tlv.value())) {
    return;
  }
  parsed_successfully_ = true;
}

X509CertificateModel::~X509CertificateModel() = default;

std::string X509CertificateModel::HashCertSHA256() const {
  auto hash =
      crypto::SHA256Hash(net::x509_util::CryptoBufferAsSpan(cert_data_.get()));
  return base::HexEncode(hash.data(), hash.size());
}

std::string X509CertificateModel::HashCertSHA256WithSeparators() const {
  auto hash =
      crypto::SHA256Hash(net::x509_util::CryptoBufferAsSpan(cert_data_.get()));
  return ProcessRawBytes(hash.data(), hash.size());
}

std::string X509CertificateModel::HashCertSHA1WithSeparators() const {
  auto hash =
      base::SHA1HashSpan(net::x509_util::CryptoBufferAsSpan(cert_data_.get()));
  return ProcessRawBytes(hash.data(), hash.size());
}

std::string X509CertificateModel::GetTitle() const {
  if (!nickname_.empty())
    return nickname_;

  if (!parsed_successfully_)
    return HashCertSHA256();

  if (!subject_rdns_.empty()) {
    OptionalStringOrError common_name = FindLastNameOfType(
        net::der::Input(net::kTypeCommonNameOid), subject_rdns_);
    if (auto* str = absl::get_if<std::string>(&common_name); str)
      return std::move(*str);
    if (absl::holds_alternative<Error>(common_name))
      return HashCertSHA256();

    std::string rv;
    if (!net::ConvertToRFC2253(subject_rdns_, &rv))
      return HashCertSHA256();
    return rv;
  }

  if (subject_alt_names_) {
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
    case net::CertificateVersion::V1:
      return "1";
    case net::CertificateVersion::V2:
      return "2";
    case net::CertificateVersion::V3:
      return "3";
  }
}

std::string X509CertificateModel::GetSerialNumberHexified() const {
  DCHECK(parsed_successfully_);
  return ProcessRawBytesWithSeparators(tbs_.serial_number.UnsafeData(),
                                       tbs_.serial_number.Length(), ':', ':');
}

bool X509CertificateModel::GetTimes(base::Time* not_before,
                                    base::Time* not_after) const {
  DCHECK(parsed_successfully_);
  return net::der::GeneralizedTimeToTime(tbs_.validity_not_before,
                                         not_before) &&
         net::der::GeneralizedTimeToTime(tbs_.validity_not_after, not_after);
}

OptionalStringOrError X509CertificateModel::GetIssuerCommonName() const {
  DCHECK(parsed_successfully_);
  // Return the last (most specific) commonName. This matches NSS
  // CERT_GetCommonName.
  return FindLastNameOfType(net::der::Input(net::kTypeCommonNameOid),
                            issuer_rdns_);
}

OptionalStringOrError X509CertificateModel::GetIssuerOrgName() const {
  DCHECK(parsed_successfully_);
  // Return the first (most general) orgName. This matches NSS CERT_GetOrgName.
  return FindFirstNameOfType(net::der::Input(net::kTypeOrganizationNameOid),
                             issuer_rdns_);
}

OptionalStringOrError X509CertificateModel::GetIssuerOrgUnitName() const {
  DCHECK(parsed_successfully_);
  // Return the first (most general) orgUnitName. This matches NSS
  // CERT_GetOrgUnitName.
  return FindFirstNameOfType(net::der::Input(net::kTypeOrganizationUnitNameOid),
                             issuer_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectCommonName() const {
  DCHECK(parsed_successfully_);
  // Return the last (most specific) commonName. This matches NSS
  // CERT_GetCommonName.
  return FindLastNameOfType(net::der::Input(net::kTypeCommonNameOid),
                            subject_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectOrgName() const {
  DCHECK(parsed_successfully_);
  // Return the first (most general) orgName. This matches NSS CERT_GetOrgName.
  return FindFirstNameOfType(net::der::Input(net::kTypeOrganizationNameOid),
                             subject_rdns_);
}

OptionalStringOrError X509CertificateModel::GetSubjectOrgUnitName() const {
  DCHECK(parsed_successfully_);
  // Return the first (most general) orgUnitName. This matches NSS
  // CERT_GetOrgUnitName.
  return FindFirstNameOfType(net::der::Input(net::kTypeOrganizationUnitNameOid),
                             subject_rdns_);
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
    base::StringPiece critical_label,
    base::StringPiece non_critical_label) const {
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
    const net::der::Input& extensions_tlv) {
  net::CertErrors unused_errors;
  net::der::Parser parser(extensions_tlv);

  //    Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
  net::der::Parser extensions_parser;
  if (!parser.ReadSequence(&extensions_parser))
    return false;

  // The Extensions SEQUENCE must contains at least 1 element (otherwise it
  // should have been omitted).
  if (!extensions_parser.HasMore())
    return false;

  while (extensions_parser.HasMore()) {
    net::ParsedExtension extension;

    net::der::Input extension_tlv;
    if (!extensions_parser.ReadRawTLV(&extension_tlv))
      return false;

    if (!ParseExtension(extension_tlv, &extension))
      return false;

    extensions_.push_back(extension);

    if (extension.oid == net::der::Input(net::kSubjectAltNameOid)) {
      subject_alt_names_ =
          net::GeneralNames::Create(extension.value, &unused_errors);
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
    base::StringPiece critical_label,
    base::StringPiece non_critical_label,
    const net::ParsedExtension& extension) const {
  base::StringPiece criticality =
      extension.critical ? critical_label : non_critical_label;
  absl::optional<std::string> processed_extension =
      ProcessExtensionData(extension);
  return base::StrCat(
      {criticality, "\n",
       (processed_extension
            ? *processed_extension
            : l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR))});
}

absl::optional<std::string> X509CertificateModel::ProcessExtensionData(
    const net::ParsedExtension& extension) const {
  if (extension.oid == net::der::Input(kNetscapeCertificateTypeOid))
    return ProcessNSCertTypeExtension(extension.value);
  if (extension.oid == net::der::Input(net::kKeyUsageOid))
    return ProcessKeyUsageExtension(extension.value);
  if (extension.oid == net::der::Input(net::kBasicConstraintsOid))
    return ProcessBasicConstraints(extension.value);
  if (extension.oid == net::der::Input(net::kExtKeyUsageOid))
    return ProcessExtKeyUsage(extension.value);
  if (extension.oid == net::der::Input(net::kSubjectAltNameOid)) {
    // The subjectAltName extension was already parsed in the constructor, use
    // that rather than parse it again.
    DCHECK(subject_alt_names_);
    return ProcessGeneralNames(*subject_alt_names_);
  }
  if (extension.oid == net::der::Input(kIssuerAltNameOid))
    return ProcessGeneralNamesTlv(extension.value);
  if (extension.oid == net::der::Input(net::kSubjectKeyIdentifierOid))
    return ProcessSubjectKeyId(extension.value);
  if (extension.oid == net::der::Input(net::kAuthorityKeyIdentifierOid))
    return ProcessAuthorityKeyId(extension.value);
  return ProcessRawBytes(extension.value);
}

// TODO(https://crbug.com/953425): move to anonymous namespace once
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

// TODO(https://crbug.com/953425): move to anonymous namespace once
// x509_certificate_model_nss is removed.
std::string ProcessRawBytesWithSeparators(const unsigned char* data,
                                          size_t data_length,
                                          char hex_separator,
                                          char line_separator) {
  static const char kHexChars[] = "0123456789ABCDEF";

  // Each input byte creates two output hex characters + a space or newline,
  // except for the last byte.
  std::string ret;
  size_t kMin = 0U;

  if (!data_length)
    return std::string();

  ret.reserve(std::max(kMin, data_length * 3 - 1));

  for (size_t i = 0; i < data_length; ++i) {
    unsigned char b = data[i];
    ret.push_back(kHexChars[(b >> 4) & 0xf]);
    ret.push_back(kHexChars[b & 0xf]);
    if (i + 1 < data_length) {
      if ((i + 1) % 16 == 0)
        ret.push_back(line_separator);
      else
        ret.push_back(hex_separator);
    }
  }
  return ret;
}

// TODO(https://crbug.com/953425): move to anonymous namespace once
// x509_certificate_model_nss is removed.
std::string ProcessRawBytes(const unsigned char* data, size_t data_length) {
  return ProcessRawBytesWithSeparators(data, data_length, ' ', '\n');
}

// TODO(https://crbug.com/953425): move to anonymous namespace once
// x509_certificate_model_nss is removed.
std::string ProcessRawBits(const unsigned char* data, size_t data_length) {
  return ProcessRawBytes(data, (data_length + 7) / 8);
}

}  // namespace x509_certificate_model
