// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_internals_utils.h"

#include "base/base64url.h"
#include "base/i18n/icubridge/date_time_formatter.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/report_request.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/enterprise/buildflags/buildflags.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/enterprise/device_trust/core/common_types.h"  // nogncheck
#include "components/enterprise/device_trust/core/device_trust_connector_service.h"  // nogncheck
#endif  // !BUILDFLAG(IS_ANDROID)
#include "components/prefs/pref_service.h"
#include "crypto/sha2.h"

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/private_key.h"  // nogncheck
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

namespace enterprise_connectors::utils {

namespace {

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
std::string BufferToString(base::span<const uint8_t> buffer) {
  return std::string(buffer.begin(), buffer.end());
}

connectors_internals::mojom::KeyTrustLevel ConvertPrivateKeySource(
    client_certificates::PrivateKeySource source) {
  switch (source) {
    case client_certificates::PrivateKeySource::kUnexportableKey:
      return connectors_internals::mojom::KeyTrustLevel::HW;
    case client_certificates::PrivateKeySource::kSoftwareKey:
      return connectors_internals::mojom::KeyTrustLevel::OS;
    case client_certificates::PrivateKeySource::kOsSoftwareKey:
      return connectors_internals::mojom::KeyTrustLevel::OS_SOFTWARE;
    case client_certificates::PrivateKeySource::kAndroidKey:
      return connectors_internals::mojom::KeyTrustLevel::HW;
    case client_certificates::PrivateKeySource::kChromeOsHwKey:
      return connectors_internals::mojom::KeyTrustLevel::HW;
    case client_certificates::PrivateKeySource::kChromeOsSwKey:
      return connectors_internals::mojom::KeyTrustLevel::OS;
  }
}

connectors_internals::mojom::LoadedKeyInfoPtr ConvertPrivateKey(
    scoped_refptr<client_certificates::PrivateKey> private_key,
    const std::optional<client_certificates::HttpCodeOrClientError>&
        key_upload_code) {
  if (!private_key) {
    return nullptr;
  }

  connectors_internals::mojom::KeyUploadStatusPtr upload_status = nullptr;
  if (key_upload_code.has_value() && key_upload_code->has_value()) {
    upload_status =
        connectors_internals::mojom::KeyUploadStatus::NewSyncKeyResponseCode(
            ToMojomValue(key_upload_code->value()));
  }

  if (key_upload_code.has_value() && !key_upload_code->has_value()) {
    upload_status =
        connectors_internals::mojom::KeyUploadStatus::NewUploadClientError(
            std::string(client_certificates::UploadClientErrorToString(
                key_upload_code->error())));
  }

  const auto spki_bytes = private_key->GetSubjectPublicKeyInfo();
  return connectors_internals::mojom::LoadedKeyInfo::New(
      ConvertPrivateKeySource(private_key->GetSource()),
      AlgorithmToType(private_key->GetAlgorithm()),
      HashAndEncodeString(BufferToString(spki_bytes)), std::move(upload_status),
      static_cast<bool>(private_key->GetSSLPrivateKey()));
}

connectors_internals::mojom::CertificateMetadataPtr ConvertCertificate(
    scoped_refptr<net::X509Certificate> certificate) {
  if (!certificate) {
    return nullptr;
  }

  using base::i18n::GetKnownLanguageTag;
  using base::i18n::IcuBridge;
  using base::i18n::datetime_options::YMD;

  return connectors_internals::mojom::CertificateMetadata::New(
      base::HexEncodeLower(certificate->serial_number()),
      base::HexEncodeLower(certificate->CalculateChainFingerprint256()),
      base::UTF16ToUTF8(IcuBridge::GetInstance().date_time_formatter().Format(
          certificate->valid_start(), GetKnownLanguageTag("en-US"),
          YMD::Medium())),
      base::UTF16ToUTF8(IcuBridge::GetInstance().date_time_formatter().Format(
          certificate->valid_expiry(), GetKnownLanguageTag("en-US"),
          YMD::Medium())),
      certificate->subject().GetDisplayName(),
      certificate->issuer().GetDisplayName());
}
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

}  // namespace

connectors_internals::mojom::KeyType AlgorithmToType(
    crypto::sign::SignatureKind algorithm) {
  switch (algorithm) {
    case crypto::sign::RSA_PKCS1_SHA1:
    case crypto::sign::RSA_PKCS1_SHA256:
    case crypto::sign::RSA_PKCS1_SHA384:
    case crypto::sign::RSA_PKCS1_SHA512:
    case crypto::sign::RSA_PSS_SHA256:
    case crypto::sign::RSA_PSS_SHA384:
    case crypto::sign::RSA_PSS_SHA512:
      return connectors_internals::mojom::KeyType::RSA;
    case crypto::sign::ECDSA_SHA1:
    case crypto::sign::ECDSA_SHA256:
    case crypto::sign::ECDSA_SHA384:
    case crypto::sign::ECDSA_SHA512:
      return connectors_internals::mojom::KeyType::EC;
    case crypto::sign::ED25519:
    case crypto::sign::MLDSA_44:
    case crypto::sign::MLDSA_65:
    case crypto::sign::MLDSA_87:
      return connectors_internals::mojom::KeyType::UNKNOWN;
  }
}

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

connectors_internals::mojom::ClientIdentityPtr GetIdentity(
    client_certificates::CertificateProvisioningService* provisioning_service,
    std::vector<std::string>& enabled_levels,
    const std::string& enabled_level) {
  const auto& status = provisioning_service->GetCurrentStatus();
  if (!(status.is_policy_enabled)) {
    return nullptr;
  }
  enabled_levels.push_back(enabled_level);

  if (!status.identity.has_value()) {
    return nullptr;
  }

  return ConvertIdentity(status.identity.value(), status.last_upload_code);
}

connectors_internals::mojom::ClientIdentityPtr ConvertIdentity(
    const client_certificates::ClientIdentity& identity,
    const std::optional<client_certificates::HttpCodeOrClientError>&
        key_upload_code) {
  return connectors_internals::mojom::ClientIdentity::New(
      identity.name, ConvertPrivateKey(identity.private_key, key_upload_code),
      ConvertCertificate(identity.certificate));
}

connectors_internals::mojom::ClientCertificateStatePtr
CreateClientCertificateState(
    client_certificates::CertificateProvisioningService*
        browser_certificate_provisioning_service,
    client_certificates::CertificateProvisioningService*
        profile_certificate_provisioning_service) {
  if (!browser_certificate_provisioning_service &&
      !profile_certificate_provisioning_service) {
    return connectors_internals::mojom::ClientCertificateState::New(
        std::vector<std::string>(), nullptr, nullptr);
  }

  std::vector<std::string> enabled_levels;
  connectors_internals::mojom::ClientIdentityPtr managed_browser_identity =
      nullptr;
  if (browser_certificate_provisioning_service) {
    managed_browser_identity =
        GetIdentity(browser_certificate_provisioning_service, enabled_levels,
                    kBrowserLevel);
  }

  connectors_internals::mojom::ClientIdentityPtr managed_profile_identity =
      nullptr;
  if (profile_certificate_provisioning_service) {
    managed_profile_identity =
        GetIdentity(profile_certificate_provisioning_service, enabled_levels,
                    kProfileLevel);
  }

  return connectors_internals::mojom::ClientCertificateState::New(
      std::move(enabled_levels), std::move(managed_profile_identity),
      std::move(managed_browser_identity));
}

#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

std::string HashAndEncodeString(const std::string& spki_bytes) {
  std::string encoded_string;
  base::Base64UrlEncode(crypto::SHA256HashString(spki_bytes),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_string);
  return encoded_string;
}

connectors_internals::mojom::Int32ValuePtr ToMojomValue(
    std::optional<int> integer_value) {
  return integer_value ? connectors_internals::mojom::Int32Value::New(
                             integer_value.value())
                       : nullptr;
}

std::string GetJsonForReportRequest(
    const enterprise_reporting::ReportRequest& request) {
  auto proto_request = request.GetChromeProfileReportRequest();

  int policy_count = 0;
  if (proto_request.has_browser_report()) {
    for (const auto& profile_info :
         proto_request.browser_report().chrome_user_profile_infos()) {
      policy_count += profile_info.chrome_policies_size();
    }
  }

  if (proto_request.has_attestation_payload() &&
      !proto_request.attestation_payload().attestation_blob().empty()) {
    proto_request.mutable_attestation_payload()->set_attestation_blob(
        "[attestation blob collected but omitted for readability]");
  }

  std::string signals_json =
      enterprise_reporting::GetSecuritySignalsInReport(proto_request);

  if (policy_count > 0) {
    std::optional<base::Value> parsed_value =
        base::JSONReader::Read(signals_json, base::JSON_PARSE_RFC);

    if (parsed_value && parsed_value->is_dict()) {
      parsed_value->GetDict().Set(
          "chrome_policies",
          base::StringPrintf(
              "[%d policies collected but omitted for readability]",
              policy_count));

      base::JSONWriter::WriteWithOptions(
          *parsed_value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &signals_json);
    }
  }

  return signals_json;
}

std::string GetStringFromTimestamp(base::Time timestamp) {
  using base::i18n::DateTimeFormatterOptions;
  using base::i18n::GetKnownLanguageTag;
  using base::i18n::IcuBridge;
  using base::i18n::datetime_options::YMDT;

  return (timestamp == base::Time())
             ? std::string()
             : base::UTF16ToUTF8(
                   IcuBridge::GetInstance().date_time_formatter().Format(
                       timestamp, GetKnownLanguageTag("en-US"),
                       YMDT::Short().with_time_precision(
                           DateTimeFormatterOptions::TimePrecision::kMinute)));
}

#if !BUILDFLAG(IS_ANDROID)
std::string ConvertPolicyLevelToString(
    enterprise_connectors::DTCPolicyLevel level) {
  switch (level) {
    case enterprise_connectors::DTCPolicyLevel::kBrowser:
      return kBrowserLevel;
    case enterprise_connectors::DTCPolicyLevel::kUser:
      return kUserLevel;
  }
  NOTREACHED();
}

std::vector<std::string> GetPolicyEnabledLevels(
    const enterprise_connectors::DeviceTrustConnectorService*
        connector_service) {
  std::vector<std::string> policy_enabled_levels;
  if (connector_service) {
    for (enterprise_connectors::DTCPolicyLevel level :
         connector_service->GetSignalsPolicyScope()) {
      policy_enabled_levels.push_back(ConvertPolicyLevelToString(level));
    }
  }
  return policy_enabled_levels;
}
#endif  // !BUILDFLAG(IS_ANDROID)

connectors_internals::mojom::DeviceTrustStatePtr
CreateUnsupportedDeviceTrustState() {
  return connectors_internals::mojom::DeviceTrustState::New(
      /*is_enabled=*/false,
      /*policy_enabled_levels=*/std::vector<std::string>(),
      /*key_info=*/
      connectors_internals::mojom::KeyInfo::New(
          connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED,
          nullptr,
          connectors_internals::mojom::KeyManagerPermanentFailure::UNSPECIFIED),
      /*signals_json=*/std::string(),
      /*consent_metadata=*/nullptr);
}

connectors_internals::mojom::DeviceTrustStatePtr
CreateDeviceTrustStateWithNoKey(
    bool is_device_trust_enabled,
    std::vector<std::string> policy_enabled_levels,
    std::string signals_json,
    connectors_internals::mojom::ConsentMetadataPtr consent_metadata) {
  return connectors_internals::mojom::DeviceTrustState::New(
      is_device_trust_enabled, std::move(policy_enabled_levels),
      connectors_internals::mojom::KeyInfo::New(
          connectors_internals::mojom::KeyManagerInitializedValue::NO_KEY,
          nullptr,
          connectors_internals::mojom::KeyManagerPermanentFailure::UNSPECIFIED),
      std::move(signals_json), std::move(consent_metadata));
}

connectors_internals::mojom::SignalsReportingStatePtr
CreateSignalsReportingState(
    const PrefService* profile_prefs,
    const enterprise_reporting::ReportScheduler* report_scheduler,
    bool can_collect_all_signals,
    std::optional<std::string> error_info) {
  std::string last_upload_attempt_time_string;
  std::string last_upload_success_time_string;
  std::string last_signals_upload_config;

  if (profile_prefs) {
    last_upload_attempt_time_string =
        GetStringFromTimestamp(profile_prefs->GetTime(
            enterprise_reporting::kLastSignalsUploadAttemptTimestamp));
    last_upload_success_time_string =
        GetStringFromTimestamp(profile_prefs->GetTime(
            enterprise_reporting::kLastSignalsUploadSucceededTimestamp));
    last_signals_upload_config = profile_prefs->GetString(
        enterprise_reporting::kLastSignalsUploadSucceededConfig);
  }

  bool status_report_enabled =
      report_scheduler && report_scheduler->IsReportingEnabled();
  bool signals_report_enabled =
      report_scheduler && report_scheduler->AreSecurityReportsEnabled();

  return connectors_internals::mojom::SignalsReportingState::New(
      std::move(error_info), status_report_enabled, signals_report_enabled,
      std::move(last_upload_attempt_time_string),
      std::move(last_upload_success_time_string),
      std::move(last_signals_upload_config), can_collect_all_signals,
      /*signals_json=*/std::nullopt);
}

std::pair<std::optional<std::string>, std::optional<std::string>>
ProcessReportGenerationResult(
    base::expected<enterprise_reporting::ReportRequestQueue,
                   enterprise_reporting::ReportGenerationError> result) {
  if (!result.has_value()) {
    return {base::StringPrintf("Report generation failed with error code: %d",
                               static_cast<int>(result.error())),
            std::nullopt};
  }
  if (result.value().empty()) {
    return {"Report generator returned an empty queue.", std::nullopt};
  }

  enterprise_reporting::ReportRequestQueue requests = std::move(result).value();
  std::unique_ptr<enterprise_reporting::ReportRequest> request =
      std::move(requests.front());
  return {std::nullopt, GetJsonForReportRequest(*request)};
}

}  // namespace enterprise_connectors::utils
