// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_model/x509_certificate_model_base.h"

#include <variant>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "components/strings/grit/components_strings.h"
#include "crypto/sha2.h"
#include "net/cert/qwac.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/certificate_policies.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_name.h"
#include "third_party/boringssl/src/pki/parse_values.h"
#include "third_party/boringssl/src/pki/parser.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"
#include "ui/base/l10n/l10n_util.h"

namespace x509_certificate_model {

namespace {

bool ParseSubjectPublicKeyInfo(bssl::der::Input spki_tlv,
                               bssl::der::Input* algorithm_tlv,
                               bssl::der::Input* subject_public_key_value) {
  bssl::der::Parser spki_parser(spki_tlv);

  //    SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //         algorithm            AlgorithmIdentifier,
  //         subjectPublicKey     BIT STRING  }
  bssl::der::Parser sequence_parser;
  if (!spki_parser.ReadSequence(&sequence_parser)) {
    return false;
  }

  if (!sequence_parser.ReadRawTLV(algorithm_tlv)) {
    return false;
  }

  if (!sequence_parser.ReadTag(CBS_ASN1_BITSTRING, subject_public_key_value)) {
    return false;
  }

  if (sequence_parser.HasMore()) {
    return false;
  }

  return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// Utility functions.

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

std::string OidToNumericString(bssl::der::Input oid) {
  CBS cbs;
  CBS_init(&cbs, oid.data(), oid.size());
  bssl::UniquePtr<char> text(CBS_asn1_oid_to_text(&cbs));
  if (!text) {
    return std::string();
  }
  return std::string("OID.") + text.get();
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

// Returns the value of the most general name of `oid` type.
// Distinguished Names are specified in least to most specific.
OptionalStringOrError FindFirstNameOfType(bssl::der::Input oid,
                                          const bssl::RDNSequence& rdns) {
  for (const bssl::RelativeDistinguishedName& rdn : rdns) {
    OptionalStringOrError r = FindAttributeOfType(oid, rdn);
    if (!std::holds_alternative<NotPresent>(r)) {
      return r;
    }
  }
  return NotPresent();
}

// Returns the value of the most specific name of `oid` type.
// Distinguished Names are specified in least to most specific.
OptionalStringOrError FindLastNameOfType(bssl::der::Input oid,
                                         const bssl::RDNSequence& rdns) {
  for (const bssl::RelativeDistinguishedName& rdn : base::Reversed(rdns)) {
    OptionalStringOrError r = FindAttributeOfType(oid, rdn);
    if (!std::holds_alternative<NotPresent>(r)) {
      return r;
    }
  }
  return NotPresent();
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
      if (!base::IsStringUTF8AllowingNoncharacters(
              base::as_string_view(value))) {
        return std::nullopt;
      }
      display_text = base::as_string_view(value);
      break;
    default:
      return std::nullopt;
  }
  // TODO(mattm): do something about newlines (or other control chars)?
  return display_text;
}

std::optional<int> GetCommonOidStringId(bssl::der::Input oid) {
  static constexpr auto kCommonOidStringMap =
      base::MakeFixedFlatMap<bssl::der::Input, int>({
          // Algorithm fields:
          {bssl::der::Input(kPkcs1RsaEncryption),
           IDS_CERT_OID_PKCS1_RSA_ENCRYPTION},
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
          {bssl::der::Input(kSecgEcSecp521r1), IDS_CERT_OID_SECG_EC_SECP521R1},
          {bssl::der::Input(kOidAlgMldsa44), IDS_CERT_OID_ML_DSA_44},
          {bssl::der::Input(kOidAlgMldsa65), IDS_CERT_OID_ML_DSA_65},
          {bssl::der::Input(kOidAlgMldsa87), IDS_CERT_OID_ML_DSA_87},

          // Extended Key Usage fields:
          {bssl::der::Input(bssl::kAnyEKU), IDS_CERT_EKU_ANY_EKU},
          {bssl::der::Input(bssl::kServerAuth),
           IDS_CERT_EKU_TLS_WEB_SERVER_AUTHENTICATION},
          {bssl::der::Input(bssl::kClientAuth),
           IDS_CERT_EKU_TLS_WEB_CLIENT_AUTHENTICATION},
          {bssl::der::Input(bssl::kCodeSigning), IDS_CERT_EKU_CODE_SIGNING},
          {bssl::der::Input(bssl::kEmailProtection),
           IDS_CERT_EKU_EMAIL_PROTECTION},
          {bssl::der::Input(bssl::kTimeStamping), IDS_CERT_EKU_TIME_STAMPING},
          {bssl::der::Input(bssl::kOCSPSigning), IDS_CERT_EKU_OCSP_SIGNING},

          // Certificate Policy Qualifier fields:
          {bssl::der::Input(bssl::kCpsPointerId),
           IDS_CERT_PKIX_CPS_POINTER_QUALIFIER},
          {bssl::der::Input(bssl::kUserNoticeId),
           IDS_CERT_PKIX_USER_NOTICE_QUALIFIER},

          // ETSI Qualified Certificate fields:
          {bssl::der::Input(net::kEtsiQcsQcComplianceOid),
           IDS_CERT_QC_ETSI_QCS_QCCOMPLIANCE},
          {bssl::der::Input(net::kEtsiQcsQcTypeOid),
           IDS_CERT_QC_ETSI_QCS_QCTYPE},
          {bssl::der::Input(net::kEtsiQctWebOid), IDS_CERT_QC_ETSI_QCT_WEB},
      });

  const auto i = kCommonOidStringMap.find(oid);
  if (i != kCommonOidStringMap.end()) {
    return i->second;
  }
  return std::nullopt;
}

namespace {

std::string GetAlgorithmOidTextOrNumeric(bssl::der::Input oid) {
  std::optional<int> common_id = GetCommonOidStringId(oid);
  if (common_id.has_value()) {
    return l10n_util::GetStringUTF8(*common_id);
  }
  return OidToNumericString(oid);
}

std::string ProcessAlgorithmIdentifier(bssl::der::Input algorithm_tlv) {
  bssl::der::Input oid;
  bssl::der::Input params;
  if (!bssl::ParseAlgorithmIdentifier(algorithm_tlv, &oid, &params)) {
    return std::string();
  }
  return GetAlgorithmOidTextOrNumeric(oid);
}

}  // namespace

// ---------------------------------------------------------------------------
// X509CertificateModelBase implementation.

X509CertificateModelBase::X509CertificateModelBase(
    bssl::UniquePtr<CRYPTO_BUFFER> cert_data)
    : cert_data_(std::move(cert_data)) {
  CHECK(cert_data_);
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

X509CertificateModelBase::X509CertificateModelBase(
    X509CertificateModelBase&& other) = default;

X509CertificateModelBase::~X509CertificateModelBase() = default;

bool X509CertificateModelBase::GetTimes(base::Time* not_before,
                                        base::Time* not_after) const {
  CHECK(is_valid());
  return net::GeneralizedTimeToTime(tbs_.validity_not_before, not_before) &&
         net::GeneralizedTimeToTime(tbs_.validity_not_after, not_after);
}

OptionalStringOrError X509CertificateModelBase::GetIssuerCommonName() const {
  CHECK(is_valid());
  // Return the last (most specific) commonName. This matches NSS
  // CERT_GetCommonName.
  return FindLastNameOfType(bssl::der::Input(bssl::kTypeCommonNameOid),
                            issuer_rdns_);
}

OptionalStringOrError X509CertificateModelBase::GetIssuerOrgName() const {
  CHECK(is_valid());
  // Return the first (most general) orgName. This matches NSS CERT_GetOrgName.
  return FindFirstNameOfType(bssl::der::Input(bssl::kTypeOrganizationNameOid),
                             issuer_rdns_);
}

OptionalStringOrError X509CertificateModelBase::GetIssuerOrgUnitName() const {
  CHECK(is_valid());
  // Return the first (most general) orgUnitName. This matches NSS
  // CERT_GetOrgUnitName.
  return FindFirstNameOfType(
      bssl::der::Input(bssl::kTypeOrganizationUnitNameOid), issuer_rdns_);
}

OptionalStringOrError X509CertificateModelBase::GetSubjectCommonName() const {
  CHECK(is_valid());
  // Return the last (most specific) commonName. This matches NSS
  // CERT_GetCommonName.
  return FindLastNameOfType(bssl::der::Input(bssl::kTypeCommonNameOid),
                            subject_rdns_);
}

OptionalStringOrError X509CertificateModelBase::GetSubjectOrgName() const {
  CHECK(is_valid());
  // Return the first (most general) orgName. This matches NSS CERT_GetOrgName.
  return FindFirstNameOfType(bssl::der::Input(bssl::kTypeOrganizationNameOid),
                             subject_rdns_);
}

OptionalStringOrError X509CertificateModelBase::GetSubjectOrgUnitName() const {
  CHECK(is_valid());
  // Return the first (most general) orgUnitName. This matches NSS
  // CERT_GetOrgUnitName.
  return FindFirstNameOfType(
      bssl::der::Input(bssl::kTypeOrganizationUnitNameOid), subject_rdns_);
}

std::string X509CertificateModelBase::HashCertSHA256() const {
  auto hash =
      crypto::SHA256Hash(net::x509_util::CryptoBufferAsSpan(cert_data_.get()));
  return base::HexEncodeLower(hash);
}

std::string X509CertificateModelBase::GetTitle() const {
  if (!is_valid()) {
    return HashCertSHA256();
  }

  if (!subject_rdns_.empty()) {
    OptionalStringOrError common_name = GetSubjectCommonName();
    if (auto* str = std::get_if<std::string>(&common_name); str) {
      return std::move(*str);
    }
    if (std::holds_alternative<Error>(common_name)) {
      return HashCertSHA256();
    }

    std::string rv;
    if (!bssl::ConvertToRFC2253(subject_rdns_, &rv)) {
      return HashCertSHA256();
    }
    return rv;
  }

  if (subject_alt_names_) {
    // TODO(mattm): do something about newlines (or other control chars)?
    if (!subject_alt_names_->dns_names.empty()) {
      return std::string(subject_alt_names_->dns_names[0]);
    }
    if (!subject_alt_names_->rfc822_names.empty()) {
      return std::string(subject_alt_names_->rfc822_names[0]);
    }
  }

  return HashCertSHA256();
}

std::string X509CertificateModelBase::HashSpkiSHA256() const {
  CHECK(is_valid());
  auto hash = crypto::SHA256Hash(tbs_.spki_tlv);
  return base::HexEncodeLower(hash);
}

std::string X509CertificateModelBase::GetVersion() const {
  CHECK(is_valid());
  switch (tbs_.version) {
    case bssl::CertificateVersion::V1:
      return "1";
    case bssl::CertificateVersion::V2:
      return "2";
    case bssl::CertificateVersion::V3:
      return "3";
  }
}

std::string X509CertificateModelBase::GetSerialNumberHexified() const {
  CHECK(is_valid());
  return ProcessRawBytesWithSeparators(tbs_.serial_number, ':', ':');
}

std::string X509CertificateModelBase::ProcessSecAlgorithmSignature() const {
  CHECK(is_valid());
  return ProcessAlgorithmIdentifier(signature_algorithm_tlv_);
}

std::string X509CertificateModelBase::ProcessSecAlgorithmSubjectPublicKey()
    const {
  CHECK(is_valid());

  bssl::der::Input algorithm_tlv;
  bssl::der::Input unused_spk_value;
  if (!ParseSubjectPublicKeyInfo(tbs_.spki_tlv, &algorithm_tlv,
                                 &unused_spk_value)) {
    return std::string();
  }

  return ProcessAlgorithmIdentifier(algorithm_tlv);
}

std::string X509CertificateModelBase::ProcessSecAlgorithmSignatureWrap() const {
  CHECK(is_valid());
  return ProcessAlgorithmIdentifier(tbs_.signature_algorithm_tlv);
}

bool X509CertificateModelBase::ParseExtensions(
    const bssl::der::Input& extensions_tlv) {
  bssl::CertErrors unused_errors;
  bssl::der::Parser parser(extensions_tlv);

  //    Extensions  ::=  SEQUENCE SIZE (1..MAX) OF Extension
  bssl::der::Parser extensions_parser;
  if (!parser.ReadSequence(&extensions_parser)) {
    return false;
  }

  // The Extensions SEQUENCE must contains at least 1 element (otherwise it
  // should have been omitted).
  if (!extensions_parser.HasMore()) {
    return false;
  }

  while (extensions_parser.HasMore()) {
    bssl::ParsedExtension extension;

    bssl::der::Input extension_tlv;
    if (!extensions_parser.ReadRawTLV(&extension_tlv)) {
      return false;
    }

    if (!bssl::ParseExtension(extension_tlv, &extension)) {
      return false;
    }

    extensions_.push_back(extension);

    if (extension.oid == bssl::der::Input(bssl::kSubjectAltNameOid)) {
      subject_alt_names_ =
          bssl::GeneralNames::Create(extension.value, &unused_errors);
      if (!subject_alt_names_) {
        return false;
      }
    }
  }

  // By definition the input was a single Extensions sequence, so there
  // shouldn't be unconsumed data.
  if (parser.HasMore()) {
    return false;
  }

  return true;
}

}  // namespace x509_certificate_model
