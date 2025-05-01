// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_page_handler.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service_factory.h"
#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals.mojom.h"
#include "chrome/browser/ui/webui/connectors_internals/device_trust_utils.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cert/x509_certificate.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#endif

namespace enterprise_connectors {

namespace {

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
constexpr char kProfile[] = "Profile";
constexpr char kBrowser[] = "Browser";
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

std::string ConvertPolicyLevelToString(DTCPolicyLevel level) {
  switch (level) {
    case DTCPolicyLevel::kBrowser:
      return "Browser";
    case DTCPolicyLevel::kUser:
      return "User";
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

  return utils::ConvertIdentity(status.identity.value(),
                                status.last_upload_code);
}
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
std::string GetStringFromTimestamp(base::Time timestamp) {
  return (timestamp == base::Time()) ? std::string()
                                     : base::UnlocalizedTimeFormatWithPattern(
                                           timestamp, "yyyy-LL-dd HH:mm zzz");
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

}  // namespace

ConnectorsInternalsPageHandler::ConnectorsInternalsPageHandler(
    mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {
  DCHECK(profile_);
}

ConnectorsInternalsPageHandler::~ConnectorsInternalsPageHandler() = default;

void ConnectorsInternalsPageHandler::GetDeviceTrustState(
    GetDeviceTrustStateCallback callback) {
  auto* device_trust_service =
      DeviceTrustServiceFactory::GetForProfile(profile_);

  // The factory will not return a service if the profile is off-the-record, or
  // if the current management configuration is not supported.
  if (!device_trust_service) {
    auto state = connectors_internals::mojom::DeviceTrustState::New(
        false, std::vector<std::string>(),
        connectors_internals::mojom::KeyInfo::New(
            connectors_internals::mojom::KeyManagerInitializedValue::
                UNSUPPORTED,
            nullptr,
            connectors_internals::mojom::KeyManagerPermanentFailure::
                UNSPECIFIED),
        std::string(), nullptr);
    std::move(callback).Run(std::move(state));
    return;
  }

  // Since this page is used for debugging purposes, show the signals regardless
  // of the policy value (i.e. even if service->IsEnabled is false).
  device_trust_service->GetSignals(
      base::BindOnce(&ConnectorsInternalsPageHandler::OnSignalsCollected,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     device_trust_service->IsEnabled()));
}

void ConnectorsInternalsPageHandler::DeleteDeviceTrustKey(
    DeleteDeviceTrustKeyCallback callback) {
#if BUILDFLAG(IS_MAC)
  auto client = SecureEnclaveClient::Create();

  // Delete both the permanent and temporary keys.
  client->DeleteKey(SecureEnclaveClient::KeyType::kTemporary);
  client->DeleteKey(SecureEnclaveClient::KeyType::kPermanent);
  std::move(callback).Run();
#else
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_MAC)
}

void ConnectorsInternalsPageHandler::GetClientCertificateState(
    GetClientCertificateStateCallback callback) {
#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
  auto* profile_certificate_provisioning_service =
      client_certificates::CertificateProvisioningServiceFactory::GetForProfile(
          profile_);
  auto* browser_certificate_provisioning_service =
      g_browser_process->browser_policy_connector()
          ->chrome_browser_cloud_management_controller()
          ->GetCertificateProvisioningService();
  if (!profile_certificate_provisioning_service &&
      !browser_certificate_provisioning_service) {
    std::move(callback).Run(
        connectors_internals::mojom::ClientCertificateState::New(
            std::vector<std::string>(), nullptr, nullptr));
    return;
  }

  std::vector<std::string> enabled_levels;
  connectors_internals::mojom::ClientIdentityPtr managed_browser_identity =
      nullptr;
  if (browser_certificate_provisioning_service) {
    managed_browser_identity = GetIdentity(
        browser_certificate_provisioning_service, enabled_levels, kBrowser);
  }

  connectors_internals::mojom::ClientIdentityPtr managed_profile_identity =
      nullptr;
  if (profile_certificate_provisioning_service) {
    managed_profile_identity = GetIdentity(
        profile_certificate_provisioning_service, enabled_levels, kProfile);
  }

  std::move(callback).Run(
      connectors_internals::mojom::ClientCertificateState::New(
          std::move(enabled_levels), std::move(managed_profile_identity),
          std::move(managed_browser_identity)));

#else
  std::move(callback).Run(
      connectors_internals::mojom::ClientCertificateState::New(
          std::vector<std::string>(), nullptr, nullptr));
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
}

void ConnectorsInternalsPageHandler::GetSignalsReportingState(
    GetSignalsReportingStateCallback callback) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  auto* profile_prefs = profile_->GetPrefs();

  std::string last_upload_attempt_time_string =
      GetStringFromTimestamp(profile_prefs->GetTime(
          enterprise_reporting::kLastSignalsUploadAttemptTimestamp));

  std::string last_upload_success_time_string =
      GetStringFromTimestamp(profile_prefs->GetTime(
          enterprise_reporting::kLastSignalsUploadSucceededTimestamp));

  std::string last_signals_upload_config = profile_prefs->GetString(
      enterprise_reporting::kLastSignalsUploadSucceededConfig);

  const auto* user_permission_service =
      enterprise_signals::UserPermissionServiceFactory::GetForProfile(profile_);
  bool can_collect_all_signals = false;
  if (user_permission_service) {
    can_collect_all_signals =
        user_permission_service->CanCollectReportSignals() ==
        device_signals::UserPermission::kGranted;
  }

  auto* profile_reporting_service =
      enterprise_reporting::CloudProfileReportingServiceFactory::GetForProfile(
          profile_);

  if (!profile_reporting_service) {
    std::move(callback).Run(
        connectors_internals::mojom::SignalsReportingState::New(
            /*error_info=*/"Profile reporting service unavailable",
            /*status_report_enabled=*/false, /*signals_report_enabled=*/false,
            last_upload_attempt_time_string, last_upload_success_time_string,
            last_signals_upload_config, can_collect_all_signals));
    return;
  }

  auto* profile_report_scheduler =
      profile_reporting_service->report_scheduler();

  if (!profile_report_scheduler) {
    std::move(callback).Run(
        connectors_internals::mojom::SignalsReportingState::New(
            /*error_info=*/"Profile report scheduler unavailable",
            /*status_report_enabled=*/false, /*signals_report_enabled=*/false,
            last_upload_attempt_time_string, last_upload_success_time_string,
            last_signals_upload_config, can_collect_all_signals));
    return;
  }

  bool status_report_enabled = profile_report_scheduler->IsReportingEnabled();
  bool signals_report_enabled =
      profile_report_scheduler->AreSecurityReportsEnabled();

  std::move(callback).Run(
      connectors_internals::mojom::SignalsReportingState::New(
          /*error_info=*/std::nullopt, status_report_enabled,
          signals_report_enabled, last_upload_attempt_time_string,
          last_upload_success_time_string, last_signals_upload_config,
          can_collect_all_signals));
#else
  std::move(callback).Run(
      connectors_internals::mojom::SignalsReportingState::New(
          /*error_info=*/"User signals reporting is unsupported on the current "
                         "platform",
          /*status_report_enabled=*/false, /*signals_report_enabled=*/false,
          /*last_upload_attempt_timestamp=*/std::string(),
          /*last_upload_success_timestamp=*/std::string(),
          /*last_signals_upload_config=*/std::string(),
          /*can_collect_all_fields=*/false));
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
}

void ConnectorsInternalsPageHandler::OnSignalsCollected(
    GetDeviceTrustStateCallback callback,
    bool is_device_trust_enabled,
    const base::Value::Dict signals) {
  std::string signals_json;
  base::JSONWriter::WriteWithOptions(
      signals, base::JSONWriter::OPTIONS_PRETTY_PRINT, &signals_json);

  const auto* user_permission_service =
      enterprise_signals::UserPermissionServiceFactory::GetForProfile(profile_);
  connectors_internals::mojom::ConsentMetadataPtr consent_metadata = nullptr;
  if (user_permission_service) {
    consent_metadata = connectors_internals::mojom::ConsentMetadata::New(
        user_permission_service->CanCollectSignals() ==
            device_signals::UserPermission::kGranted,
        user_permission_service->HasUserConsented());
  }

  std::vector<std::string> policy_enabled_levels;
  auto* device_trust_connector_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(profile_);
  if (device_trust_connector_service) {
    for (const auto& level :
         device_trust_connector_service->GetEnabledInlinePolicyLevels()) {
      policy_enabled_levels.push_back(ConvertPolicyLevelToString(level));
    }
  }

  auto state = connectors_internals::mojom::DeviceTrustState::New(
      is_device_trust_enabled, policy_enabled_levels, utils::GetKeyInfo(),
      signals_json, std::move(consent_metadata));
  std::move(callback).Run(std::move(state));
}

}  // namespace enterprise_connectors
