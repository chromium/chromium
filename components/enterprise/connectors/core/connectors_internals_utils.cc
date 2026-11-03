// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/connectors/core/connectors_internals_utils.h"

#include "base/base64url.h"
#include "base/i18n/icubridge/date_time_formatter.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/time_formatting.h"
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
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/device_trust/core/common_types.h"  // nogncheck
#include "components/enterprise/device_trust/core/device_trust_connector_service.h"  // nogncheck
#include "components/prefs/pref_service.h"
#include "crypto/sha2.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"

#if BUILDFLAG(ENTERPRISE_PROXY)
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#endif  // BUILDFLAG(ENTERPRISE_PROXY)

namespace enterprise_connectors::utils {

namespace {

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

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter(
          GetKnownLanguageTag("en-US"));

  return connectors_internals::mojom::CertificateMetadata::New(
      base::HexEncodeLower(certificate->serial_number()),
      base::HexEncodeLower(certificate->CalculateChainFingerprint256()),
      base::UTF16ToUTF8(
          formatter.Format(certificate->valid_start(), YMD::Medium())),
      base::UTF16ToUTF8(
          formatter.Format(certificate->valid_expiry(), YMD::Medium())),
      certificate->subject().GetDisplayName(),
      certificate->issuer().GetDisplayName());
}

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

base::DictValue GetSignalsForReportRequest(
    const enterprise_reporting::ReportRequest& request) {
  return enterprise_reporting::GetSecuritySignalsDictInReport(
      request.GetChromeProfileReportRequest(),
      /*include_policy_summary=*/true);
}

std::string GetStringFromTimestamp(base::Time timestamp) {
  using base::i18n::DateTimeFormatterOptions;
  using base::i18n::GetKnownLanguageTag;
  using base::i18n::IcuBridge;
  using base::i18n::datetime_options::YMDT;

  return (timestamp == base::Time())
             ? std::string()
             : base::UTF16ToUTF8(
                   IcuBridge::GetInstance()
                       .date_time_formatter(GetKnownLanguageTag("en-US"))
                       .Format(timestamp, YMDT::Short().with_time_precision(
                                              DateTimeFormatterOptions::
                                                  TimePrecision::kMinute)));
}

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

  // Prefer showing exactly what was last uploaded, as some signals (e.g. client
  // certificates) are only collected as part of an upload. Cached signals are
  // not shown when signals reporting is disabled.
  connectors_internals::mojom::SignalsPtr signals;
  if (signals_report_enabled && report_scheduler->GetLastUploadedSignals()) {
    signals = connectors_internals::mojom::Signals::New(
        report_scheduler->GetLastUploadedSignals()->Clone(),
        connectors_internals::mojom::SignalsSource::kLastUploadedReport);
  }

  return connectors_internals::mojom::SignalsReportingState::New(
      std::move(error_info), status_report_enabled, signals_report_enabled,
      std::move(last_upload_attempt_time_string),
      std::move(last_upload_success_time_string),
      std::move(last_signals_upload_config), can_collect_all_signals,
      std::move(signals));
}

std::pair<std::optional<std::string>, std::optional<base::DictValue>>
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
  return {std::nullopt, GetSignalsForReportRequest(*request)};
}

#if BUILDFLAG(ENTERPRISE_PROXY)

connectors_internals::mojom::ProvisioningDomainStatePtr
GetProvisioningDomainState(
    enterprise_net::EnterpriseProxyService* proxy_service) {
  std::vector<connectors_internals::mojom::ProvisioningDomainConfigPtr>
      pvd_configs;
  if (!proxy_service) {
    return connectors_internals::mojom::ProvisioningDomainState::New(
        std::move(pvd_configs));
  }

  base::DictValue debug_info = proxy_service->GetDebugInfo();
  const base::ListValue* domains = debug_info.FindList("domains");

  if (!domains) {
    return connectors_internals::mojom::ProvisioningDomainState::New(
        std::move(pvd_configs));
  }

  for (const base::Value& domain : *domains) {
    if (!domain.is_dict()) {
      continue;
    }
    const base::DictValue& domain_dict = domain.GetDict();

    const std::string* pvd_id_ptr =
        domain_dict.FindStringByDottedPath("policy.pvd_id");
    if (!pvd_id_ptr || pvd_id_ptr->empty()) {
      pvd_id_ptr =
          domain_dict.FindStringByDottedPath("fetched_config.identifier");
    }
    std::string pvd_id = pvd_id_ptr ? *pvd_id_ptr : "";

    const std::string* expires_ptr =
        domain_dict.FindStringByDottedPath("fetched_config.expires");
    std::optional<base::Time> expires_time;
    if (expires_ptr) {
      base::Time parsed_time;
      if (base::Time::FromString(expires_ptr->c_str(), &parsed_time)) {
        expires_time = parsed_time;
      }
    }

    const base::DictValue* policy_dict = domain_dict.FindDict("policy");
    std::string policy_json;
    if (policy_dict) {
      base::JSONWriter::WriteWithOptions(
          *policy_dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &policy_json);
    }

    const base::DictValue* fetched_dict =
        domain_dict.FindDict("fetched_config");
    const base::DictValue* last_failure_dict =
        domain_dict.FindDict("last_failure");
    std::string routes_json;
    if (fetched_dict) {
      if (last_failure_dict) {
        base::DictValue routes_dict = fetched_dict->Clone();
        routes_dict.Set("last_failure", last_failure_dict->Clone());
        base::JSONWriter::WriteWithOptions(
            routes_dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &routes_json);
      } else {
        base::JSONWriter::WriteWithOptions(
            *fetched_dict, base::JSONWriter::OPTIONS_PRETTY_PRINT,
            &routes_json);
      }
    }

    pvd_configs.push_back(
        connectors_internals::mojom::ProvisioningDomainConfig::New(
            std::move(pvd_id), expires_time, std::move(routes_json),
            std::move(policy_json)));
  }

  return connectors_internals::mojom::ProvisioningDomainState::New(
      std::move(pvd_configs));
}

#endif  // BUILDFLAG(ENTERPRISE_PROXY)
}  // namespace enterprise_connectors::utils
