// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_page_handler.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service_factory.h"
#include "chrome/browser/enterprise/signals/signals_aggregator_factory.h"
#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/device_signals/core/browser/signals_aggregator.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/connectors/connectors_internals.mojom.h"
#include "components/enterprise/connectors/core/connectors_internals_utils.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cert/x509_certificate.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_android.h"
#else
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#include "chrome/browser/ui/webui/connectors_internals/device_trust_utils.h"
#include "components/enterprise/device_trust/core/device_trust_service.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif

#if BUILDFLAG(ENTERPRISE_PROXY)
#include "chrome/browser/enterprise/net/enterprise_proxy_service_factory.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#endif

namespace enterprise_connectors {

ConnectorsInternalsPageHandler::ConnectorsInternalsPageHandler(
    mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {
  DCHECK(profile_);
}

ConnectorsInternalsPageHandler::~ConnectorsInternalsPageHandler() = default;

void ConnectorsInternalsPageHandler::GetDeviceTrustState(
    GetDeviceTrustStateCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED();
#else
  auto* device_trust_service =
      DeviceTrustServiceFactory::GetForProfile(profile_);

  // The factory will not return a service if the profile is off-the-record, or
  // if the current management configuration is not supported.
  if (!device_trust_service) {
    std::move(callback).Run(utils::CreateUnsupportedDeviceTrustState());
    return;
  }

  // Since this page is used for debugging purposes, show the signals regardless
  // of the policy value (i.e. even if service->IsEnabled is false).
  device_trust_service->GetSignals(
      base::BindOnce(&ConnectorsInternalsPageHandler::OnSignalsCollected,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     device_trust_service->IsEnabled()));
#endif  // BUILDFLAG(IS_ANDROID)
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
  client_certificates::CertificateProvisioningService*
      browser_certificate_provisioning_service = nullptr;
#if !BUILDFLAG(IS_CHROMEOS)
  // Browser-level (machine) certificate provisioning is driven by the Chrome
  // Browser Cloud Management controller, which does not exist on ChromeOS.
  browser_certificate_provisioning_service =
      g_browser_process->browser_policy_connector()
          ->chrome_browser_cloud_management_controller()
          ->GetCertificateProvisioningService();
#endif  // !BUILDFLAG(IS_CHROMEOS)
  std::move(callback).Run(utils::CreateClientCertificateState(
      browser_certificate_provisioning_service,
      profile_certificate_provisioning_service));

#else
  std::move(callback).Run(
      connectors_internals::mojom::ClientCertificateState::New(
          std::vector<std::string>(), nullptr, nullptr));
#endif
}

void ConnectorsInternalsPageHandler::GetSignalsReportingState(
    GetSignalsReportingStateCallback callback) {
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
    std::move(callback).Run(utils::CreateSignalsReportingState(
        profile_->GetPrefs(), /*report_scheduler=*/nullptr,
        can_collect_all_signals,
        /*error_info=*/"Profile reporting service unavailable"));
    return;
  }

  auto* profile_report_scheduler =
      profile_reporting_service->report_scheduler();

  if (!profile_report_scheduler) {
    std::move(callback).Run(utils::CreateSignalsReportingState(
        profile_->GetPrefs(), /*report_scheduler=*/nullptr,
        can_collect_all_signals,
        /*error_info=*/"Profile report scheduler unavailable"));
    return;
  }

  auto state = utils::CreateSignalsReportingState(
      profile_->GetPrefs(), profile_report_scheduler, can_collect_all_signals);

  auto* signals_aggregator =
      enterprise_signals::SignalsAggregatorFactory::GetForProfile(profile_);

  if (!state->signals_report_enabled || !signals_aggregator) {
    std::move(callback).Run(std::move(state));
    return;
  }

#if BUILDFLAG(IS_ANDROID)
  auto delegate_factory =
      std::make_unique<enterprise_reporting::ReportingDelegateFactoryAndroid>();
#else
  auto delegate_factory =
      std::make_unique<enterprise_reporting::ReportingDelegateFactoryDesktop>();
#endif

  request_generator_ =
      std::make_unique<enterprise_reporting::ChromeProfileRequestGenerator>(
          profile_->GetPath(), delegate_factory.get(), signals_aggregator);

  enterprise_reporting::ReportGenerationConfig config;
  config.report_type = enterprise_reporting::ReportType::kProfileReport;
  config.security_signals_mode = SecuritySignalsMode::kSignalsAttached;

  request_generator_->Generate(
      std::move(config),
      base::BindOnce(&ConnectorsInternalsPageHandler::OnReportGenerated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(state)));
}

void ConnectorsInternalsPageHandler::GetProvisioningDomainState(
    GetProvisioningDomainStateCallback callback) {
  std::vector<connectors_internals::mojom::ProvisioningDomainConfigPtr>
      pvd_configs;
#if BUILDFLAG(ENTERPRISE_PROXY)
  auto* proxy_service = EnterpriseProxyServiceFactory::GetForProfile(profile_);
  if (!proxy_service) {
    std::move(callback).Run(
        connectors_internals::mojom::ProvisioningDomainState::New(
            std::move(pvd_configs)));
    return;
  }

  base::DictValue debug_info = proxy_service->GetDebugInfo();
  const base::ListValue* domains = debug_info.FindList("domains");

  if (!domains) {
    std::move(callback).Run(
        connectors_internals::mojom::ProvisioningDomainState::New(
            std::move(pvd_configs)));
    return;
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
    std::string routes_json;
    if (fetched_dict) {
      base::JSONWriter::WriteWithOptions(
          *fetched_dict, base::JSONWriter::OPTIONS_PRETTY_PRINT, &routes_json);
    }

    pvd_configs.push_back(
        connectors_internals::mojom::ProvisioningDomainConfig::New(
            std::move(pvd_id), expires_time, std::move(routes_json),
            std::move(policy_json)));
  }
#endif  // BUILDFLAG(ENTERPRISE_PROXY)

  std::move(callback).Run(
      connectors_internals::mojom::ProvisioningDomainState::New(
          std::move(pvd_configs)));
}

#if !BUILDFLAG(IS_ANDROID)
void ConnectorsInternalsPageHandler::OnSignalsCollected(
    GetDeviceTrustStateCallback callback,
    bool is_device_trust_enabled,
    const base::DictValue signals) {
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

  auto* device_trust_connector_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(profile_);
  std::vector<std::string> policy_enabled_levels =
      utils::GetPolicyEnabledLevels(device_trust_connector_service);

  auto state = connectors_internals::mojom::DeviceTrustState::New(
      is_device_trust_enabled, std::move(policy_enabled_levels),
      utils::GetKeyInfo(), std::move(signals_json),
      std::move(consent_metadata));
  std::move(callback).Run(std::move(state));
}
#endif  // !BUILDFLAG(IS_ANDROID)

void ConnectorsInternalsPageHandler::OnReportGenerated(
    GetSignalsReportingStateCallback callback,
    connectors_internals::mojom::SignalsReportingStatePtr state,
    base::expected<enterprise_reporting::ReportRequestQueue,
                   enterprise_reporting::ReportGenerationError> result) {
  auto [error_info, signals_json] =
      utils::ProcessReportGenerationResult(std::move(result));
  state->error_info = std::move(error_info);
  state->signals_json = std::move(signals_json);
  std::move(callback).Run(std::move(state));
  request_generator_.reset();
}

}  // namespace enterprise_connectors
