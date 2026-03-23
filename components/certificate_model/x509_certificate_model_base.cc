// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_model/x509_certificate_model_base.h"

#include <variant>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "components/strings/grit/components_strings.h"
#include "net/cert/time_conversions.h"
#include "third_party/boringssl/src/pki/certificate_policies.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_name.h"
#include "third_party/boringssl/src/pki/parse_values.h"

namespace x509_certificate_model {

// ---------------------------------------------------------------------------
// Utility functions.

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
    if (!std::holds_alternative<NotPresent>(r)) {
      return r;
    }
  }
  return NotPresent();
}

// Returns the value of the most specific name of |oid| type.
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
  static const auto kCommonOidStringMap =
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

          // Extended Key Usage fields:
          {bssl::der::Input(bssl::kAnyEKU), IDS_CERT_EKU_ANY_EKU},
          {bssl::der::Input(bssl::kCodeSigning), IDS_CERT_EKU_CODE_SIGNING},
          {bssl::der::Input(bssl::kEmailProtection),
           IDS_CERT_EKU_EMAIL_PROTECTION},
          {bssl::der::Input(bssl::kTimeStamping), IDS_CERT_EKU_TIME_STAMPING},

          // Certificate Policy Qualifier fields:
          {bssl::der::Input(bssl::kCpsPointerId),
           IDS_CERT_PKIX_CPS_POINTER_QUALIFIER},
          {bssl::der::Input(bssl::kUserNoticeId),
           IDS_CERT_PKIX_USER_NOTICE_QUALIFIER},
      });

  const auto i = kCommonOidStringMap.find(oid);
  if (i != kCommonOidStringMap.end()) {
    return i->second;
  }
  return std::nullopt;
}

// ---------------------------------------------------------------------------
// X509CertificateModelBase implementation.

X509CertificateModelBase::X509CertificateModelBase(
    bssl::UniquePtr<CRYPTO_BUFFER> cert_data)
    : cert_data_(std::move(cert_data)) {
  CHECK(cert_data_);
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

}  // namespace x509_certificate_model
