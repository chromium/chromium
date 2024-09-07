// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui_handler_chromeos.h"

#include "base/check_is_test.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/management/management_ui_constants.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/net/secure_dns_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/status_collector/device_status_collector.h"
#include "chrome/browser/ash/policy/status_collector/status_collector.h"
#include "chrome/browser/ash/policy/uploading/status_uploader.h"
#include "chrome/browser/ash/policy/uploading/system_log_uploader.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/management/management_ui_handler_chromeos.h"
#include "chrome/grit/branded_strings.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"
#include "ui/chromeos/devicetype_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/strings/escape.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

const char kAccountManagedInfo[] = "accountManagedInfo";
const char kDeviceManagedInfo[] = "deviceManagedInfo";
const char kOverview[] = "overview";

enum class DeviceReportingType {
  kSupervisedUser,
  kDeviceActivity,
  kDeviceStatistics,
  kDevice,
  kCrashReport,
  kAppInfoAndActivity,
  kLogs,
  kPrint,
  kPrintJobs,
  kCrostini,
  kUsername,
  kExtensions,
  kAndroidApplication,
  kDlpEvents,
  kLoginLogout,
  kCRDSessions,
  kPeripherals,
  kLegacyTech,
  kWebsiteInfoAndActivity,
  kFileEvents,
};

#if BUILDFLAG(IS_CHROMEOS_ASH)

const char kCustomerLogo[] = "customerLogo";

net::NetworkTrafficAnnotationTag GetManagementUICustomerLogoAnnotation() {
  return net::DefineNetworkTrafficAnnotation("management_ui_customer_logo", R"(
      semantics {
        sender: "Management UI Handler"
        description:
          "Download organization logo for visualization on the "
          "chrome://management page."
        trigger:
          "The user managed by organization that provides a company logo "
          "in their GSuites account loads the chrome://management page."
        data:
          "Organization uploaded image URL."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature cannot be disabled by settings, but it is only "
          "triggered by a user action."
        policy_exception_justification: "Not implemented."
      })");
}

// Corresponds to DeviceReportingType in management_browser_proxy.js
std::string ToJSDeviceReportingType(const DeviceReportingType& type) {
  switch (type) {
    case DeviceReportingType::kSupervisedUser:
      return "supervised user";
    case DeviceReportingType::kDeviceActivity:
      return "device activity";
    case DeviceReportingType::kDeviceStatistics:
      return "device statistics";
    case DeviceReportingType::kDevice:
      return "device";
    case DeviceReportingType::kCrashReport:
      return "crash report";
    case DeviceReportingType::kAppInfoAndActivity:
      return "app info and activity";
    case DeviceReportingType::kLogs:
      return "logs";
    case DeviceReportingType::kPrint:
      return "print";
    case DeviceReportingType::kPrintJobs:
      return "print jobs";
    case DeviceReportingType::kCrostini:
      return "crostini";
    case DeviceReportingType::kUsername:
      return "username";
    case DeviceReportingType::kExtensions:
      return "extension";
    case DeviceReportingType::kAndroidApplication:
      return "android application";
    case DeviceReportingType::kDlpEvents:
      return "dlp events";
    case DeviceReportingType::kLoginLogout:
      return "login-logout";
    case DeviceReportingType::kCRDSessions:
      return "crd sessions";
    case DeviceReportingType::kPeripherals:
      return "peripherals";
    case DeviceReportingType::kLegacyTech:
      return kReportingTypeLegacyTech;
    case DeviceReportingType::kWebsiteInfoAndActivity:
      return "website info and activity";
    case DeviceReportingType::kFileEvents:
      return "file events";
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown device reporting type";
      return "device";
  }
}

std::string GetWebsiteReportingAllowlistMessageParam(
    const base::Value::List& url_allowlist) {
  std::vector<std::string> url_patterns;
  for (const base::Value& pattern_value : url_allowlist) {
    url_patterns.push_back(pattern_value.GetString());
  }

  return base::JoinString(url_patterns, ", ");
}

void AddDeviceReportingElement(
    base::Value::List* report_sources,
    const std::string& message_id,
    const DeviceReportingType& type,
    base::Value::List message_params = base::Value::List()) {
  base::Value::Dict data;
  data.Set("messageId", message_id);
  data.Set("reportingType", ToJSDeviceReportingType(type));
  data.Set("messageParams", std::move(message_params));

  report_sources->Append(std::move(data));
}

const policy::DlpRulesManager* GetDlpRulesManager() {
  return policy::DlpRulesManagerFactory::GetForPrimaryProfile();
}

// If you are adding a privacy note, please also add it to
// go/chrome-policy-privacy-note-mappings.
void AddDeviceReportingInfo(base::Value::List* report_sources,
                            const policy::StatusCollector* collector,
                            const policy::SystemLogUploader* uploader,
                            Profile* profile) {
  if (!collector || !profile || !uploader) {
    return;
  }

  // Elements appear on the page in the order they are added.
  bool report_device_peripherals = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kReportDevicePeripherals,
                                       &report_device_peripherals);
  bool report_audio_status = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kReportDeviceAudioStatus,
                                       &report_audio_status);
  // TODO(b/262295601): Add/refine management strings corresponding to XDR
  // reporting policy.
  bool device_report_xdr_events = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kDeviceReportXDREvents,
                                       &device_report_xdr_events);
  if (collector->IsReportingActivityTimes() || report_device_peripherals ||
      report_audio_status || device_report_xdr_events ||
      profile->GetPrefs()->GetBoolean(::prefs::kInsightsExtensionEnabled)) {
    AddDeviceReportingElement(report_sources, kManagementReportActivityTimes,
                              DeviceReportingType::kDeviceActivity);
  } else {
    if (collector->IsReportingUsers()) {
      AddDeviceReportingElement(report_sources, kManagementReportUsers,
                                DeviceReportingType::kSupervisedUser);
    }
  }
  if (collector->IsReportingNetworkData() ||
      profile->GetPrefs()->GetBoolean(::prefs::kInsightsExtensionEnabled)) {
    AddDeviceReportingElement(report_sources, kManagementReportNetworkData,
                              DeviceReportingType::kDevice);
  }
  if (collector->IsReportingHardwareData()) {
    AddDeviceReportingElement(report_sources, kManagementReportHardwareData,
                              DeviceReportingType::kDeviceStatistics);
  }
  if (collector->IsReportingCrashReportInfo()) {
    AddDeviceReportingElement(report_sources, kManagementReportCrashReports,
                              DeviceReportingType::kCrashReport);
  }

  const auto& app_inventory_app_types =
      profile->GetPrefs()->GetList(::ash::reporting::kReportAppInventory);
  const auto& app_usage_app_types =
      profile->GetPrefs()->GetList(::ash::reporting::kReportAppUsage);
  if (collector->IsReportingAppInfoAndActivity() || device_report_xdr_events ||
      !app_inventory_app_types.empty() || !app_usage_app_types.empty()) {
    AddDeviceReportingElement(report_sources,
                              kManagementReportAppInfoAndActivity,
                              DeviceReportingType::kAppInfoAndActivity);
  }
  if (uploader->upload_enabled()) {
    AddDeviceReportingElement(report_sources, kManagementLogUploadEnabled,
                              DeviceReportingType::kLogs);
  }

  if (report_audio_status) {
    AddDeviceReportingElement(report_sources,
                              kManagementReportDeviceAudioStatus,
                              DeviceReportingType::kDevice);
  }

  bool report_graphics_status = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kReportDeviceGraphicsStatus,
                                       &report_graphics_status);
  if (report_graphics_status) {
    AddDeviceReportingElement(report_sources,
                              kManagementReportDeviceGraphicsStatus,
                              DeviceReportingType::kDevice);
  }

  if (report_device_peripherals) {
    AddDeviceReportingElement(report_sources,
                              kManagementReportDevicePeripherals,
                              DeviceReportingType::kPeripherals);
  }

  bool report_print_jobs = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kReportDevicePrintJobs,
                                       &report_print_jobs);
  if (report_print_jobs) {
    AddDeviceReportingElement(report_sources, kManagementReportPrintJobs,
                              DeviceReportingType::kPrintJobs);
  }

  bool report_print_username = profile->GetPrefs()->GetBoolean(
      prefs::kPrintingSendUsernameAndFilenameEnabled);
  if (report_print_username && !report_print_jobs) {
    AddDeviceReportingElement(report_sources, kManagementPrinting,
                              DeviceReportingType::kPrint);
  }

  if (GetDlpRulesManager() && GetDlpRulesManager()->IsReportingEnabled()) {
    AddDeviceReportingElement(report_sources, kManagementReportDlpEvents,
                              DeviceReportingType::kDlpEvents);
  }

  if (crostini::CrostiniFeatures::Get()->IsAllowedNow(profile)) {
    if (!profile->GetPrefs()
             ->GetFilePath(crostini::prefs::kCrostiniAnsiblePlaybookFilePath)
             .empty()) {
      AddDeviceReportingElement(report_sources,
                                kManagementCrostiniContainerConfiguration,
                                DeviceReportingType::kCrostini);
    } else if (profile->GetPrefs()->GetBoolean(
                   crostini::prefs::kReportCrostiniUsageEnabled)) {
      AddDeviceReportingElement(report_sources, kManagementCrostini,
                                DeviceReportingType::kCrostini);
    }
  }

  if (g_browser_process->local_state()->GetBoolean(
          enterprise_reporting::kCloudReportingEnabled)) {
    AddDeviceReportingElement(report_sources,
                              kManagementExtensionReportUsername,
                              DeviceReportingType::kUsername);
    AddDeviceReportingElement(report_sources, kManagementReportExtensions,
                              DeviceReportingType::kExtensions);
    AddDeviceReportingElement(report_sources,
                              kManagementReportAndroidApplications,
                              DeviceReportingType::kAndroidApplication);
  }

  if (!profile->GetPrefs()
           ->GetList(enterprise_reporting::kCloudLegacyTechReportAllowlist)
           .empty()) {
    AddDeviceReportingElement(report_sources, kManagementLegacyTechReport,
                              DeviceReportingType::kLegacyTech);
  }

  bool report_login_logout = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kReportDeviceLoginLogout,
                                       &report_login_logout);
  bool report_xdr_events = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kDeviceReportXDREvents,
                                       &report_xdr_events);
  if (report_login_logout || report_xdr_events) {
    AddDeviceReportingElement(report_sources, kManagementReportLoginLogout,
                              DeviceReportingType::kLoginLogout);
  }

  if (report_xdr_events) {
    AddDeviceReportingElement(report_sources, kManagementReportFileEvents,
                              DeviceReportingType::kFileEvents);
  }

  bool report_crd_sessions = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kReportCRDSessions,
                                       &report_crd_sessions);
  if (report_crd_sessions) {
    AddDeviceReportingElement(report_sources, kManagementReportCRDSessions,
                              DeviceReportingType::kCRDSessions);
  }

  const auto wildcard_pattern_string =
      ContentSettingsPattern::Wildcard().ToString();
  const auto& website_telemetry_types =
      profile->GetPrefs()->GetList(::reporting::kReportWebsiteTelemetry);
  const auto& website_telemetry_allowlist = profile->GetPrefs()->GetList(
      ::reporting::kReportWebsiteTelemetryAllowlist);
  const auto& website_activity_allowlist = profile->GetPrefs()->GetList(
      ::reporting::kReportWebsiteActivityAllowlist);
  if (base::Contains(website_activity_allowlist, wildcard_pattern_string) ||
      (!website_telemetry_types.empty() &&
       base::Contains(website_telemetry_allowlist, wildcard_pattern_string))) {
    // One or more website metrics reporting policies allowlists all website
    // URLs.
    AddDeviceReportingElement(report_sources,
                              kManagementReportAllWebsiteInfoAndActivity,
                              DeviceReportingType::kWebsiteInfoAndActivity);
  } else if (!website_activity_allowlist.empty()) {
    // Admin defined subset of URLs allowlisted for website activity reporting.
    base::Value::List message_params;
    message_params.Append(
        GetWebsiteReportingAllowlistMessageParam(website_activity_allowlist));
    AddDeviceReportingElement(report_sources,
                              kManagementReportWebsiteInfoAndActivity,
                              DeviceReportingType::kWebsiteInfoAndActivity,
                              std::move(message_params));
  } else if (!website_telemetry_types.empty() &&
             !website_telemetry_allowlist.empty()) {
    // Admin defined subset of URLs allowlisted for website telemetry reporting.
    base::Value::List message_params;
    message_params.Append(
        GetWebsiteReportingAllowlistMessageParam(website_telemetry_allowlist));
    AddDeviceReportingElement(report_sources,
                              kManagementReportWebsiteInfoAndActivity,
                              DeviceReportingType::kWebsiteInfoAndActivity,
                              std::move(message_params));
  }
}

void AddStatusOverviewManagedDeviceAndAccount(
    base::Value::Dict* status,
    bool device_managed,
    bool account_managed,
    const std::string& device_manager,
    const std::string& account_manager) {
  if (device_managed && account_managed &&
      (account_manager.empty() || account_manager == device_manager)) {
    status->Set(kOverview, base::Value(l10n_util::GetStringFUTF16(
                               IDS_MANAGEMENT_DEVICE_AND_ACCOUNT_MANAGED_BY,
                               base::UTF8ToUTF16(device_manager))));

    return;
  }

  if (account_managed && !account_manager.empty()) {
    status->Set(kOverview, base::Value(l10n_util::GetStringFUTF16(
                               IDS_MANAGEMENT_ACCOUNT_MANAGED_BY,
                               base::UTF8ToUTF16(account_manager))));
  }

  if (account_managed && device_managed && !account_manager.empty() &&
      account_manager != device_manager) {
    status->Set(kOverview,
                base::Value(l10n_util::GetStringFUTF16(
                    IDS_MANAGEMENT_DEVICE_MANAGED_BY_ACCOUNT_MANAGED_BY,
                    base::UTF8ToUTF16(device_manager),
                    base::UTF8ToUTF16(account_manager))));
  }
}

bool IsCloudDestination(policy::local_user_files::FileSaveDestination dest) {
  return dest == policy::local_user_files::FileSaveDestination::kGoogleDrive ||
         dest == policy::local_user_files::FileSaveDestination::kOneDrive;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

ManagementUIHandlerChromeOS::ManagementUIHandlerChromeOS(Profile* profile)
    : ManagementUIHandler(profile) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  // Get device report sources.
  if (service->IsAvailable<crosapi::mojom::DeviceSettingsService>() &&
      service->GetInterfaceVersion<crosapi::mojom::DeviceSettingsService>() >=
          static_cast<int>(crosapi::mojom::DeviceSettingsService::
                               kGetDeviceReportSourcesMinVersion)) {
    service->GetRemote<crosapi::mojom::DeviceSettingsService>()
        ->GetDeviceReportSources(base::BindOnce(
            &ManagementUIHandlerChromeOS::OnGotDeviceReportSources,
            weak_factory_.GetWeakPtr()));
  }
#endif
  // profile is unset during unittest, in which case, we can initial device
  // managed state in ctor either. Hence skip the process.
  if (!profile) {
    CHECK_IS_TEST();
    return;
  }
  UpdateDeviceManagedState();
}

ManagementUIHandlerChromeOS::~ManagementUIHandlerChromeOS() = default;

void ManagementUIHandlerChromeOS::RegisterMessages() {
  ManagementUIHandler::RegisterMessages();
  web_ui()->RegisterMessageCallback(
      "getLocalTrustRootsInfo",
      base::BindRepeating(
          &ManagementUIHandlerChromeOS::HandleGetLocalTrustRootsInfo,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDeviceReportingInfo",
      base::BindRepeating(
          &ManagementUIHandlerChromeOS::HandleGetDeviceReportingInfo,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPluginVmDataCollectionStatus",
      base::BindRepeating(
          &ManagementUIHandlerChromeOS::HandleGetPluginVmDataCollectionStatus,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getFilesUploadToCloudInfo",
      base::BindRepeating(
          &ManagementUIHandlerChromeOS::HandleGetFilesUploadToCloudInfo,
          base::Unretained(this)));

  capture_policy::CheckGetAllScreensMediaAllowedForAnyOrigin(
      Profile::FromWebUI(web_ui()),
      base::BindOnce(
          &ManagementUIHandlerChromeOS::
              CheckGetAllScreensMediaAllowedForAnyOriginResultReceived,
          weak_factory_.GetWeakPtr()));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

// static
base::Value::List ManagementUIHandlerChromeOS::GetDeviceReportingInfo(
    const policy::DeviceCloudPolicyManagerAsh* manager,
    Profile* profile) {
  base::Value::List report_sources;
  policy::StatusUploader* uploader = nullptr;
  policy::SystemLogUploader* syslog_uploader = nullptr;
  policy::StatusCollector* collector = nullptr;
  if (manager) {
    uploader = manager->GetStatusUploader();
    syslog_uploader = manager->GetSystemLogUploader();
    if (uploader) {
      collector = uploader->status_collector();
    }
  }
  AddDeviceReportingInfo(&report_sources, collector, syslog_uploader, profile);
  return report_sources;
}

// static
void ManagementUIHandlerChromeOS::AddDlpDeviceReportingElementForTesting(
    base::Value::List* report_sources,
    const std::string& message_id) {
  AddDeviceReportingElement(report_sources, message_id,
                            DeviceReportingType::kDlpEvents);
}

// static
void ManagementUIHandlerChromeOS::AddDeviceReportingInfoForTesting(
    base::Value::List* report_sources,
    const policy::StatusCollector* collector,
    const policy::SystemLogUploader* uploader,
    Profile* profile) {
  AddDeviceReportingInfo(report_sources, collector, uploader, profile);
}

const policy::DeviceCloudPolicyManagerAsh*
ManagementUIHandlerChromeOS::GetDeviceCloudPolicyManager() const {
  // Only check for report status in managed environment.
  if (!device_managed_) {
    return nullptr;
  }

  const policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->GetDeviceCloudPolicyManager();
}

const std::string ManagementUIHandlerChromeOS::GetDeviceManager() const {
  std::string device_domain;
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (device_managed_) {
    device_domain = connector->GetEnterpriseDomainManager();
  }
  return device_domain;
}

bool ManagementUIHandlerChromeOS::IsUpdateRequiredEol() const {
  const policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::MinimumVersionPolicyHandler* handler =
      connector->GetMinimumVersionPolicyHandler();
  return handler && handler->ShouldShowUpdateRequiredEolBanner();
}

void ManagementUIHandlerChromeOS::AddUpdateRequiredEolInfo(
    base::Value::Dict* response) const {
  if (!device_managed_ || !IsUpdateRequiredEol()) {
    response->Set("eolMessage", std::string());
    return;
  }

  response->Set("eolMessage", l10n_util::GetStringFUTF16(
                                  IDS_MANAGEMENT_UPDATE_REQUIRED_EOL_MESSAGE,
                                  base::UTF8ToUTF16(GetDeviceManager()),
                                  ui::GetChromeOSDeviceName()));
  std::string eol_admin_message;
  ash::CrosSettings::Get()->GetString(ash::kDeviceMinimumVersionAueMessage,
                                      &eol_admin_message);
  response->Set("eolAdminMessage", eol_admin_message);
}

void ManagementUIHandlerChromeOS::AddMonitoredNetworkPrivacyDisclosure(
    base::Value::Dict* response) {
  bool showMonitoredNetworkDisclosure = false;

  // Check for secure DNS templates with identifiers.
  std::optional<std::string> doh_with_identifiers_servers_for_display;

  doh_with_identifiers_servers_for_display =
      GetSecureDnsManager()->GetDohWithIdentifiersDisplayServers();

  showMonitoredNetworkDisclosure =
      doh_with_identifiers_servers_for_display.has_value();
  if (showMonitoredNetworkDisclosure) {
    response->Set("showMonitoredNetworkPrivacyDisclosure",
                  showMonitoredNetworkDisclosure);
    return;
  }

  // Check if DeviceReportXDREvents is enabled.
  auto* report_xdr_events_policy_value =
      GetPolicyService()
          ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                std::string()))
          .GetValue(policy::key::kDeviceReportXDREvents,
                    base::Value::Type::BOOLEAN);
  bool report_xdr_events_policy_enabled =
      report_xdr_events_policy_value &&
      report_xdr_events_policy_value->GetBool();

  if (report_xdr_events_policy_enabled) {
    response->Set("showMonitoredNetworkPrivacyDisclosure",
                  report_xdr_events_policy_enabled);
    return;
  }

  // Check for proxy config.
  ash::NetworkHandler* network_handler = ash::NetworkHandler::Get();
  base::Value::Dict proxy_settings;
  // |ui_proxy_config_service| may be missing in tests. If the device is offline
  // (no network connected) the |DefaultNetwork| is null.
  if (ash::NetworkHandler::HasUiProxyConfigService() &&
      network_handler->network_state_handler()->DefaultNetwork()) {
    // Check if proxy is enforced by user policy, a forced install extension or
    // ONC policies. This will only read managed settings.
    ash::NetworkHandler::GetUiProxyConfigService()->MergeEnforcedProxyConfig(
        network_handler->network_state_handler()->DefaultNetwork()->guid(),
        &proxy_settings);
  }
  if (!proxy_settings.empty()) {
    // Proxies can be specified by web server url, via a PAC script or via the
    // web proxy auto-discovery protocol. Chrome also supports the "direct"
    // mode, in which no proxy is used.
    std::string* proxy_specification_mode =
        proxy_settings.FindStringByDottedPath(base::JoinString(
            {::onc::network_config::kType, ::onc::kAugmentationActiveSetting},
            "."));
    showMonitoredNetworkDisclosure =
        proxy_specification_mode &&
        *proxy_specification_mode != ::onc::proxy::kDirect;
  }
  response->Set("showMonitoredNetworkPrivacyDisclosure",
                showMonitoredNetworkDisclosure);
}

void ManagementUIHandlerChromeOS::RegisterPrefChange(
    PrefChangeRegistrar& pref_registrar) {
  ManagementUIHandler::RegisterPrefChange(pref_registrar);
  pref_registrar.Add(
      plugin_vm::prefs::kPluginVmDataCollectionAllowed,
      base::BindRepeating(
          &ManagementUIHandlerChromeOS::NotifyPluginVmDataCollectionUpdated,
          base::Unretained(this)));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

base::Value::Dict ManagementUIHandlerChromeOS::GetContextualManagedData(
    Profile* profile) {
  base::Value::Dict response;
  std::string enterprise_manager;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  enterprise_manager = GetDeviceManager();
#endif
  if (enterprise_manager.empty()) {
    enterprise_manager = GetAccountManager(profile);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  AddUpdateRequiredEolInfo(&response);
  AddMonitoredNetworkPrivacyDisclosure(&response);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  response.Set("pageSubtitle", chrome::GetManagementPageSubtitle(profile));
  response.Set("browserManagementNotice",
               l10n_util::GetStringFUTF16(
                   managed() ? IDS_MANAGEMENT_BROWSER_NOTICE
                             : IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                   chrome::kManagedUiLearnMoreUrl,
                   base::EscapeForHTML(l10n_util::GetStringUTF16(
                       IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
#endif
  if (enterprise_manager.empty()) {
    response.Set(
        "extensionReportingSubtitle",
        l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
    response.Set(
        "applicationReportingSubtitle",
        l10n_util::GetStringUTF16(IDS_MANAGEMENT_APPLICATIONS_INSTALLED));
    response.Set(
        "managedWebsitesSubtitle",
        l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));
#if BUILDFLAG(IS_CHROMEOS_ASH)
    const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
    response.Set("pageSubtitle",
                 managed() ? l10n_util::GetStringFUTF16(
                                 IDS_MANAGEMENT_SUBTITLE_MANAGED,
                                 l10n_util::GetStringUTF16(device_type))
                           : l10n_util::GetStringFUTF16(
                                 IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                 l10n_util::GetStringUTF16(device_type)));
#endif
  } else {
    response.Set(
        "extensionReportingSubtitle",
        l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                   base::UTF8ToUTF16(enterprise_manager)));
    response.Set(
        "applicationReportingSubtitle",
        l10n_util::GetStringFUTF16(IDS_MANAGEMENT_APPLICATIONS_INSTALLED_BY,
                                   base::UTF8ToUTF16(enterprise_manager)));
    response.Set("managedWebsitesSubtitle",
                 l10n_util::GetStringFUTF16(
                     IDS_MANAGEMENT_MANAGED_WEBSITES_BY_EXPLANATION,
                     base::UTF8ToUTF16(enterprise_manager)));
#if BUILDFLAG(IS_CHROMEOS_ASH)
    const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
    response.Set("pageSubtitle",
                 managed() ? l10n_util::GetStringFUTF16(
                                 IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                 l10n_util::GetStringUTF16(device_type),
                                 base::UTF8ToUTF16(enterprise_manager))
                           : l10n_util::GetStringFUTF16(
                                 IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                 l10n_util::GetStringUTF16(device_type)));
#endif
  }
  response.Set("managed", managed());
  GetManagementStatus(profile, &response);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  AsyncUpdateLogo();
  if (!fetched_image_.empty()) {
    response.Set(kCustomerLogo, base::Value(fetched_image_));
  }
#endif

  return response;
}

bool ManagementUIHandlerChromeOS::managed() const {
  return account_managed() || device_managed_;
}

void ManagementUIHandlerChromeOS::UpdateManagedState() {
  bool is_account_updated =
      UpdateAccountManagedState(Profile::FromWebUI(web_ui()));
  bool is_device_updated = UpdateDeviceManagedState();
  if (is_account_updated || is_device_updated) {
    FireWebUIListener("managed_data_changed");
  }
}

bool ManagementUIHandlerChromeOS::UpdateDeviceManagedState() {
  bool new_managed =
      policy::ManagementServiceFactory::GetForPlatform()->IsManaged();
  bool is_updated = (device_managed_ != new_managed);
  device_managed_ = new_managed;
  return is_updated;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ManagementUIHandlerChromeOS::AsyncUpdateLogo() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  const auto url = connector->GetCustomerLogoURL();
  if (!url.empty() && GURL(url) != logo_url_) {
    icon_fetcher_ = std::make_unique<BitmapFetcher>(
        GURL(url), this, GetManagementUICustomerLogoAnnotation());
    icon_fetcher_->Init(net::ReferrerPolicy::NEVER_CLEAR,
                        network::mojom::CredentialsMode::kOmit);
    auto* profile = Profile::FromWebUI(web_ui());
    icon_fetcher_->Start(profile->GetDefaultStoragePartition()
                             ->GetURLLoaderFactoryForBrowserProcess()
                             .get());
  }
}

void ManagementUIHandlerChromeOS::OnFetchComplete(const GURL& url,
                                                  const SkBitmap* bitmap) {
  if (!bitmap) {
    return;
  }
  fetched_image_ = webui::GetBitmapDataUrl(*bitmap);
  logo_url_ = url;
  // Fire listener to reload managed data.
  FireWebUIListener("managed_data_changed");
}

void ManagementUIHandlerChromeOS::NotifyPluginVmDataCollectionUpdated() {
  FireWebUIListener(
      "plugin-vm-data-collection-updated",
      base::Value(Profile::FromWebUI(web_ui())->GetPrefs()->GetBoolean(
          plugin_vm::prefs::kPluginVmDataCollectionAllowed)));
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ManagementUIHandlerChromeOS::OnGotDeviceReportSources(
    base::Value::List report_sources,
    bool plugin_vm_data_collection_enabled) {
  report_sources_ = std::move(report_sources);
  plugin_vm_data_collection_enabled_ = plugin_vm_data_collection_enabled;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void ManagementUIHandlerChromeOS::GetManagementStatus(
    Profile* profile,
    base::Value::Dict* status) const {
  status->Set(kDeviceManagedInfo, base::Value());
  status->Set(kAccountManagedInfo, base::Value());
  status->Set(kOverview, base::Value());
  if (!managed()) {
    status->Set(kOverview, base::Value(l10n_util::GetStringUTF16(
                               IDS_MANAGEMENT_DEVICE_NOT_MANAGED)));
    return;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string account_manager = GetAccountManager(profile);
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  auto* primary_profile =
      primary_user ? ash::ProfileHelper::Get()->GetProfileByUser(primary_user)
                   : nullptr;
  const bool primary_user_managed =
      primary_profile ? IsProfileManaged(primary_profile) : false;

  if (primary_user_managed) {
    account_manager = GetAccountManager(primary_profile);
  }

  std::string device_manager = GetDeviceManager();

  AddStatusOverviewManagedDeviceAndAccount(
      status, device_managed_, account_managed() || primary_user_managed,
      device_manager, account_manager);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ManagementUIHandlerChromeOS::HandleGetLocalTrustRootsInfo(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  base::Value trust_roots_configured(false);
  AllowJavascript();

  policy::PolicyCertService* policy_service =
      policy::PolicyCertServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (policy_service && policy_service->has_policy_certificates()) {
    trust_roots_configured = base::Value(true);
  }

  ResolveJavascriptCallback(args[0] /* callback_id */, trust_roots_configured);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::u16string ManagementUIHandlerChromeOS::GetFilesUploadToCloudInfo(
    Profile* profile) {
  policy::local_user_files::FileSaveDestination download_destination =
      policy::local_user_files::GetDownloadsDestination(profile);
  policy::local_user_files::FileSaveDestination screenshot_destination =
      policy::local_user_files::GetScreenCaptureDestination(profile);
  int uploads_id = -1;
  int destination_id = -1;
  if (IsCloudDestination(download_destination) &&
      IsCloudDestination(screenshot_destination)) {
    uploads_id = IDS_MANAGEMENT_FILES_CLOUD_UPLOAD_DOWNLOADS_AND_SCREENSHOTS;
    if (download_destination ==
            policy::local_user_files::FileSaveDestination::kGoogleDrive &&
        screenshot_destination ==
            policy::local_user_files::FileSaveDestination::kGoogleDrive) {
      destination_id = IDS_MANAGEMENT_FILES_CLOUD_UPLOAD_GOOGLE_DRIVE;
    } else if (download_destination ==
                   policy::local_user_files::FileSaveDestination::kOneDrive &&
               screenshot_destination ==
                   policy::local_user_files::FileSaveDestination::kOneDrive) {
      destination_id = IDS_MANAGEMENT_FILES_CLOUD_UPLOAD_ONEDRIVE;
    } else {
      destination_id =
          IDS_MANAGEMENT_FILES_CLOUD_UPLOAD_GOOGLE_DRIVE_AND_ONEDRIVE;
    }
  } else if (IsCloudDestination(download_destination)) {
    uploads_id = IDS_MANAGEMENT_FILES_CLOUD_UPLOAD_DOWNLOADS;
    if (download_destination ==
        policy::local_user_files::FileSaveDestination::kGoogleDrive) {
      destination_id = IDS_MANAGEMENT_FILES_CLOUD_UPLOAD_GOOGLE_DRIVE;
    } else {
      destination_id = IDS_MANAGEMENT_FILES_CLOUD_UPLOAD_ONEDRIVE;
    }
  } else if (IsCloudDestination(screenshot_destination)) {
    uploads_id = IDS_MANAGEMENT_FILES_CLOUD_UPLOAD_SCREENSHOTS;
    if (screenshot_destination ==
        policy::local_user_files::FileSaveDestination::kGoogleDrive) {
      destination_id = IDS_MANAGEMENT_FILES_CLOUD_UPLOAD_GOOGLE_DRIVE;
    } else {
      destination_id = IDS_MANAGEMENT_FILES_CLOUD_UPLOAD_ONEDRIVE;
    }
  }

  if (uploads_id == -1) {
    return std::u16string();
  }

  return l10n_util::GetStringFUTF16(
      IDS_MANAGEMENT_FILES_CLOUD_UPLOAD_CONFIGURATION,
      l10n_util::GetStringUTF16(uploads_id),
      l10n_util::GetStringUTF16(destination_id));
}

const ash::SecureDnsManager* ManagementUIHandlerChromeOS::GetSecureDnsManager()
    const {
  return g_browser_process->platform_part()->secure_dns_manager();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void ManagementUIHandlerChromeOS::HandleGetFilesUploadToCloudInfo(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());

  AllowJavascript();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ResolveJavascriptCallback(
      args[0] /* callback_id */,
      base::Value(GetFilesUploadToCloudInfo(Profile::FromWebUI(web_ui()))));
#else
  ResolveJavascriptCallback(args[0] /* callback_id */, base::Value());
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ManagementUIHandlerChromeOS::HandleGetDeviceReportingInfo(
    const base::Value::List& args) {
  AllowJavascript();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::Value::List report_sources = GetDeviceReportingInfo(
      GetDeviceCloudPolicyManager(), Profile::FromWebUI(web_ui()));
  ResolveJavascriptCallback(args[0] /* callback_id */, report_sources);
#else
  ResolveJavascriptCallback(args[0] /* callback_id */, report_sources_);
#endif
}

void ManagementUIHandlerChromeOS::HandleGetPluginVmDataCollectionStatus(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::Value plugin_vm_data_collection_enabled(
      Profile::FromWebUI(web_ui())->GetPrefs()->GetBoolean(
          plugin_vm::prefs::kPluginVmDataCollectionAllowed));
#else
  base::Value plugin_vm_data_collection_enabled(
      plugin_vm_data_collection_enabled_);
#endif
  AllowJavascript();
  ResolveJavascriptCallback(args[0] /* callback_id */,
                            plugin_vm_data_collection_enabled);
}

void ManagementUIHandlerChromeOS::
    CheckGetAllScreensMediaAllowedForAnyOriginResultReceived(bool is_allowed) {
  set_is_get_all_screens_media_allowed_for_any_origin(is_allowed);
  if (IsJavascriptAllowed()) {
    NotifyThreatProtectionInfoUpdated();
  }
}

std::unique_ptr<ManagementUIHandler> ManagementUIHandler::Create(
    Profile* profile) {
  return std::make_unique<ManagementUIHandlerChromeOS>(profile);
}
