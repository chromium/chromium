// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/hash/sha1.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "crypto/sha2.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/parse_name.h"
#include "net/cert/x509_util.h"
#include "net/der/encode_values.h"
#include "net/der/input.h"
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

// The certificate viewer may be used to view client certificates, so use the
// relaxed parsing mode. See crbug.com/770323 and crbug.com/788655.
constexpr auto kNameStringHandling =
    net::X509NameAttribute::PrintableStringHandling::kAsUTF8Hack;

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
});

std::string GetOidText(net::der::Input oid) {
  // TODO(crbug.com/1311404): this should be "const auto i" since it's an
  // iterator, but fixed_flat_map iterators are raw pointers and the
  // chromium-style plugin complains.
  const auto* i = kOidStringMap.find(oid);
  if (i != kOidStringMap.end())
    return l10n_util::GetStringUTF8(i->second);

  return OidToNumericString(oid);
}

std::string ProcessRDN(const net::RelativeDistinguishedName& rdn) {
  std::string rv;
  // In X.509, RelativeDistinguishedName is a Set, so "last" has no meaning,
  // and generally only has one element anyway.  Just traverse in encoded
  // order.
  for (const net::X509NameAttribute& name_attribute : rdn) {
    std::string oid_text = GetOidText(name_attribute.type);
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
  if (tbs_.has_extensions && !ParseExtensions(tbs_.extensions_tlv)) {
    return;
  }
  parsed_successfully_ = true;
}

X509CertificateModel::~X509CertificateModel() = default;

std::string X509CertificateModel::HashCertSHA256() const {
  auto hash =
      crypto::SHA256Hash(net::x509_util::CryptoBufferAsSpan(cert_data_.get()));
  return ProcessRawBytes(hash.data(), hash.size());
}

std::string X509CertificateModel::HashCertSHA1() const {
  auto hash =
      base::SHA1HashSpan(net::x509_util::CryptoBufferAsSpan(cert_data_.get()));
  return ProcessRawBytes(hash.data(), hash.size());
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
