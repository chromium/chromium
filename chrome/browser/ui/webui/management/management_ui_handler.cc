// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui_handler.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/status_collector/device_status_collector.h"
#include "chrome/browser/chromeos/policy/status_collector/status_collector.h"
#include "chrome/browser/chromeos/policy/status_uploader.h"
#include "chrome/browser/chromeos/policy/system_log_uploader.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/ui/webui/management/management_ui_handler_chromeos.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/chromium_strings.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/proxy/proxy_config_handler.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/devicetype_utils.h"
#else
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/permissions/permissions_data.h"

const char kOnPremReportingExtensionStableId[] =
    "emahakmocgideepebncgnmlmliepgpgb";
const char kOnPremReportingExtensionBetaId[] =
    "kigjhoekjcpdfjpimbdjegmgecmlicaf";
const char kPolicyKeyReportMachineIdData[] = "report_machine_id_data";
const char kPolicyKeyReportUserIdData[] = "report_user_id_data";
const char kPolicyKeyReportVersionData[] = "report_version_data";
const char kPolicyKeyReportPolicyData[] = "report_policy_data";
const char kPolicyKeyReportExtensionsData[] = "report_extensions_data";
const char kPolicyKeyReportSystemTelemetryData[] =
    "report_system_telemetry_data";
const char kPolicyKeyReportUserBrowsingData[] = "report_user_browsing_data";

const char kManagementExtensionReportMachineName[] =
    "managementExtensionReportMachineName";
const char kManagementExtensionReportMachineNameAddress[] =
    "managementExtensionReportMachineNameAddress";
const char kManagementExtensionReportUsername[] =
    "managementExtensionReportUsername";
const char kManagementExtensionReportVersion[] =
    "managementExtensionReportVersion";
const char kManagementExtensionReportExtensionsPlugin[] =
    "managementExtensionReportExtensionsPlugin";
const char kManagementExtensionReportPerfCrash[] =
    "managementExtensionReportPerfCrash";
const char kManagementExtensionReportUserBrowsingData[] =
    "managementExtensionReportUserBrowsingData";

const char kThreatProtectionTitle[] = "threatProtectionTitle";
const char kManagementDataLossPreventionName[] =
    "managementDataLossPreventionName";
const char kManagementDataLossPreventionPermissions[] =
    "managementDataLossPreventionPermissions";
const char kManagementMalwareScanningName[] = "managementMalwareScanningName";
const char kManagementMalwareScanningPermissions[] =
    "managementMalwareScanningPermissions";
const char kManagementEnterpriseReportingEvent[] =
    "managementEnterpriseReportingEvent";
const char kManagementEnterpriseReportingVisibleData[] =
    "managementEnterpriseReportingVisibleData";

const char kManagementOnFileAttachedEvent[] = "managementOnFileAttachedEvent";
const char kManagementOnFileAttachedVisibleData[] =
    "managementOnFileAttachedVisibleData";
const char kManagementOnFileDownloadedEvent[] =
    "managementOnFileDownloadedEvent";
const char kManagementOnFileDownloadedVisibleData[] =
    "managementOnFileDownloadedVisibleData";
const char kManagementOnBulkDataEntryEvent[] = "managementOnBulkDataEntryEvent";
const char kManagementOnBulkDataEntryVisibleData[] =
    "managementOnBulkDataEntryVisibleData";

const char kManagementOnPageVisitedEvent[] = "managementOnPageVisitedEvent";
const char kManagementOnPageVisitedVisibleData[] =
    "managementOnPageVisitedVisibleData";

const char kReportingTypeDevice[] = "device";
const char kReportingTypeExtensions[] = "extensions";
const char kReportingTypeSecurity[] = "security";
const char kReportingTypeUser[] = "user";
const char kReportingTypeUserActivity[] = "user-activity";

enum class ReportingType {
  kDevice,
  kExtensions,
  kSecurity,
  kUser,
  kUserActivity
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kManagementLogUploadEnabled[] = "managementLogUploadEnabled";
const char kManagementReportActivityTimes[] = "managementReportActivityTimes";
const char kManagementReportHardwareStatus[] = "managementReportHardwareStatus";
const char kManagementReportNetworkInterfaces[] =
    "managementReportNetworkInterfaces";
const char kManagementReportUsers[] = "managementReportUsers";
const char kManagementReportCrashReports[] = "managementReportCrashReports";
const char kManagementReportAppInfoAndActivity[] =
    "managementReportAppInfoAndActivity";
const char kManagementReportExtensions[] = "managementReportExtensions";
const char kManagementReportAndroidApplications[] =
    "managementReportAndroidApplications";
const char kManagementReportPrintJobs[] = "managementReportPrintJobs";
const char kManagementPrinting[] = "managementPrinting";
const char kManagementCrostini[] = "managementCrostini";
const char kManagementCrostiniContainerConfiguration[] =
    "managementCrostiniContainerConfiguration";
const char kAccountManagedInfo[] = "accountManagedInfo";
const char kDeviceManagedInfo[] = "deviceManagedInfo";
const char kOverview[] = "overview";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kCustomerLogo[] = "customerLogo";

const char kPowerfulExtensionsCountHistogram[] = "Extensions.PowerfulCount";

namespace {

bool IsProfileManaged(Profile* profile) {
  return profile->GetProfilePolicyConnector()->IsManaged();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsDeviceManaged() {
  return webui::IsEnterpriseManaged();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_ASH)
bool IsBrowserManaged() {
  return g_browser_process->browser_policy_connector()
      ->HasMachineLevelPolicies();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)

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
  kAndroidApplication
};

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
    default:
      NOTREACHED() << "Unknown device reporting type";
      return "device";
  }
}

void AddDeviceReportingElement(base::Value* report_sources,
                               const std::string& message_id,
                               const DeviceReportingType& type) {
  base::Value data(base::Value::Type::DICTIONARY);
  data.SetKey("messageId", base::Value(message_id));
  data.SetKey("reportingType", base::Value(ToJSDeviceReportingType(type)));
  report_sources->Append(std::move(data));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::vector<base::Value> GetPermissionsForExtension(
    scoped_refptr<const extensions::Extension> extension) {
  std::vector<base::Value> permission_messages;
  // Only consider force installed extensions
  if (!extensions::Manifest::IsPolicyLocation(extension->location()))
    return permission_messages;

  extensions::PermissionIDSet permissions =
      extensions::PermissionMessageProvider::Get()
          ->GetManagementUIPermissionIDs(
              extension->permissions_data()->active_permissions(),
              extension->GetType());

  const extensions::PermissionMessages messages =
      extensions::PermissionMessageProvider::Get()->GetPermissionMessages(
          permissions);

  for (const auto& message : messages)
    permission_messages.push_back(base::Value(message.message()));

  return permission_messages;
}

base::Value GetPowerfulExtensions(const extensions::ExtensionSet& extensions) {
  base::Value powerful_extensions(base::Value::Type::LIST);

  for (const auto& extension : extensions) {
    std::vector<base::Value> permission_messages =
        GetPermissionsForExtension(extension);

    // Only show extension on page if there is at least one permission
    // message to show.
    if (!permission_messages.empty()) {
      std::unique_ptr<base::DictionaryValue> extension_to_add =
          extensions::util::GetExtensionInfo(extension.get());
      extension_to_add->SetKey("permissions",
                               base::Value(std::move(permission_messages)));
      powerful_extensions.Append(std::move(*extension_to_add));
    }
  }

  return powerful_extensions;
}

const char* GetReportingTypeValue(ReportingType reportingType) {
  switch (reportingType) {
    case ReportingType::kDevice:
      return kReportingTypeDevice;
    case ReportingType::kExtensions:
      return kReportingTypeExtensions;
    case ReportingType::kSecurity:
      return kReportingTypeSecurity;
    case ReportingType::kUser:
      return kReportingTypeUser;
    case ReportingType::kUserActivity:
      return kReportingTypeUserActivity;
    default:
      return kReportingTypeSecurity;
  }
}

}  // namespace

std::string ManagementUIHandler::GetAccountManager(Profile* profile) {
  base::Optional<std::string> account_manager =
      chrome::GetAccountManagerIdentity(profile);
  return account_manager ? *account_manager : std::string();
}

ManagementUIHandler::ManagementUIHandler() {
  reporting_extension_ids_ = {kOnPremReportingExtensionStableId,
                              kOnPremReportingExtensionBetaId};
}

ManagementUIHandler::~ManagementUIHandler() {
  DisallowJavascript();
}

void ManagementUIHandler::Initialize(content::WebUI* web_ui,
                                     content::WebUIDataSource* source) {
  InitializeInternal(web_ui, source, Profile::FromWebUI(web_ui));
}
// static
void ManagementUIHandler::InitializeInternal(content::WebUI* web_ui,
                                             content::WebUIDataSource* source,
                                             Profile* profile) {
  auto handler = std::make_unique<ManagementUIHandler>();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  handler->account_managed_ = IsProfileManaged(profile);
  handler->device_managed_ = IsDeviceManaged();
#else
  handler->account_managed_ = IsProfileManaged(profile) || IsBrowserManaged();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  web_ui->AddMessageHandler(std::move(handler));
}

void ManagementUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getContextualManagedData",
      base::BindRepeating(&ManagementUIHandler::HandleGetContextualManagedData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExtensions",
      base::BindRepeating(&ManagementUIHandler::HandleGetExtensions,
                          base::Unretained(this)));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "getLocalTrustRootsInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetLocalTrustRootsInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDeviceReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetDeviceReportingInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPluginVmDataCollectionStatus",
      base::BindRepeating(
          &ManagementUIHandler::HandleGetPluginVmDataCollectionStatus,
          base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "getThreatProtectionInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetThreatProtectionInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getManagedWebsites",
      base::BindRepeating(&ManagementUIHandler::HandleGetManagedWebsites,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initBrowserReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleInitBrowserReportingInfo,
                          base::Unretained(this)));
}

void ManagementUIHandler::OnJavascriptAllowed() {
  AddObservers();
}

void ManagementUIHandler::OnJavascriptDisallowed() {
  RemoveObservers();
}

void ManagementUIHandler::AddReportingInfo(base::Value* report_sources) {
  const policy::PolicyService* policy_service = GetPolicyService();

  const policy::PolicyNamespace
      on_prem_reporting_extension_stable_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionStableId);
  const policy::PolicyNamespace
      on_prem_reporting_extension_beta_policy_namespace =
          policy::PolicyNamespace(policy::POLICY_DOMAIN_EXTENSIONS,
                                  kOnPremReportingExtensionBetaId);

  const policy::PolicyMap& on_prem_reporting_extension_stable_policy_map =
      policy_service->GetPolicies(
          on_prem_reporting_extension_stable_policy_namespace);
  const policy::PolicyMap& on_prem_reporting_extension_beta_policy_map =
      policy_service->GetPolicies(
          on_prem_reporting_extension_beta_policy_namespace);

  const policy::PolicyMap* policy_maps[] = {
      &on_prem_reporting_extension_stable_policy_map,
      &on_prem_reporting_extension_beta_policy_map};

  const auto* cloud_reporting_policy_value =
      GetPolicyService()
          ->GetPolicies(policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME,
                                                std::string()))
          .GetValue(policy::key::kCloudReportingEnabled);
  const bool cloud_reporting_policy_enabled =
      cloud_reporting_policy_value && cloud_reporting_policy_value->is_bool() &&
      cloud_reporting_policy_value->GetBool();

  const struct {
    const char* policy_key;
    const char* message;
    const ReportingType reporting_type;
    const bool enabled_by_default;
  } report_definitions[] = {
      {kPolicyKeyReportMachineIdData, kManagementExtensionReportMachineName,
       ReportingType::kDevice, cloud_reporting_policy_enabled},
      {kPolicyKeyReportMachineIdData,
       kManagementExtensionReportMachineNameAddress, ReportingType::kDevice,
       false},
      {kPolicyKeyReportVersionData, kManagementExtensionReportVersion,
       ReportingType::kDevice, cloud_reporting_policy_enabled},
      {kPolicyKeyReportSystemTelemetryData, kManagementExtensionReportPerfCrash,
       ReportingType::kDevice, false},
      {kPolicyKeyReportUserIdData, kManagementExtensionReportUsername,
       ReportingType::kUser, cloud_reporting_policy_enabled},
      {kPolicyKeyReportExtensionsData,
       kManagementExtensionReportExtensionsPlugin, ReportingType::kExtensions,
       cloud_reporting_policy_enabled},
      {kPolicyKeyReportUserBrowsingData,
       kManagementExtensionReportUserBrowsingData, ReportingType::kUserActivity,
       false},
  };

  std::unordered_set<const char*> enabled_messages;

  for (auto& report_definition : report_definitions) {
    if (report_definition.enabled_by_default) {
      enabled_messages.insert(report_definition.message);
    } else if (report_definition.policy_key) {
      for (const policy::PolicyMap* policy_map : policy_maps) {
        const base::Value* policy_value =
            policy_map->GetValue(report_definition.policy_key);
        if (policy_value && policy_value->is_bool() &&
            policy_value->GetBool()) {
          enabled_messages.insert(report_definition.message);
          break;
        }
      }
    }
  }

  // The message with more data collected for kPolicyKeyReportMachineIdData
  // trumps the one with less data.
  if (enabled_messages.find(kManagementExtensionReportMachineNameAddress) !=
      enabled_messages.end()) {
    enabled_messages.erase(kManagementExtensionReportMachineName);
  }

  for (auto& report_definition : report_definitions) {
    if (enabled_messages.find(report_definition.message) ==
        enabled_messages.end()) {
      continue;
    }

    base::Value data(base::Value::Type::DICTIONARY);
    data.SetKey("messageId", base::Value(report_definition.message));
    data.SetKey(
        "reportingType",
        base::Value(GetReportingTypeValue(report_definition.reporting_type)));
    report_sources->Append(std::move(data));
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
const policy::DeviceCloudPolicyManagerChromeOS*
ManagementUIHandler::GetDeviceCloudPolicyManager() const {
  // Only check for report status in managed environment.
  if (!device_managed_)
    return nullptr;

  const policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->GetDeviceCloudPolicyManager();
}

void ManagementUIHandler::AddDeviceReportingInfo(
    base::Value* report_sources,
    const policy::StatusCollector* collector,
    const policy::SystemLogUploader* uploader,
    Profile* profile) const {
  if (!collector || !profile || !uploader)
    return;

  // Elements appear on the page in the order they are added.
  if (collector->ShouldReportActivityTimes()) {
    AddDeviceReportingElement(report_sources, kManagementReportActivityTimes,
                              DeviceReportingType::kDeviceActivity);
  } else {
    if (collector->ShouldReportUsers()) {
      AddDeviceReportingElement(report_sources, kManagementReportUsers,
                                DeviceReportingType::kSupervisedUser);
    }
  }
  if (collector->ShouldReportHardwareStatus()) {
    AddDeviceReportingElement(report_sources, kManagementReportHardwareStatus,
                              DeviceReportingType::kDeviceStatistics);
  }
  if (collector->ShouldReportNetworkInterfaces()) {
    AddDeviceReportingElement(report_sources,
                              kManagementReportNetworkInterfaces,
                              DeviceReportingType::kDevice);
  }
  if (collector->ShouldReportCrashReportInfo()) {
    AddDeviceReportingElement(report_sources, kManagementReportCrashReports,
                              DeviceReportingType::kCrashReport);
  }
  if (collector->ShouldReportAppInfoAndActivity()) {
    AddDeviceReportingElement(report_sources,
                              kManagementReportAppInfoAndActivity,
                              DeviceReportingType::kAppInfoAndActivity);
  }
  if (uploader->upload_enabled()) {
    AddDeviceReportingElement(report_sources, kManagementLogUploadEnabled,
                              DeviceReportingType::kLogs);
  }

  bool report_print_jobs = false;
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kReportDevicePrintJobs,
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
          enterprise_reporting::kCloudReportingEnabled) &&
      base::FeatureList::IsEnabled(features::kEnterpriseReportingInChromeOS)) {
    AddDeviceReportingElement(report_sources,
                              kManagementExtensionReportUsername,
                              DeviceReportingType::kUsername);
    AddDeviceReportingElement(report_sources, kManagementReportExtensions,
                              DeviceReportingType::kExtensions);
    AddDeviceReportingElement(report_sources,
                              kManagementReportAndroidApplications,
                              DeviceReportingType::kAndroidApplication);
  }
}

bool ManagementUIHandler::IsUpdateRequiredEol() const {
  const policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::MinimumVersionPolicyHandler* handler =
      connector->GetMinimumVersionPolicyHandler();
  return handler && handler->ShouldShowUpdateRequiredEolBanner();
}

void ManagementUIHandler::AddUpdateRequiredEolInfo(
    base::Value* response) const {
  if (!device_managed_ || !IsUpdateRequiredEol()) {
    response->SetStringPath("eolMessage", std::string());
    return;
  }

  response->SetStringPath(
      "eolMessage",
      l10n_util::GetStringFUTF16(IDS_MANAGEMENT_UPDATE_REQUIRED_EOL_MESSAGE,
                                 base::UTF8ToUTF16(GetDeviceManager()),
                                 ui::GetChromeOSDeviceName()));
  std::string eol_admin_message;
  ash::CrosSettings::Get()->GetString(chromeos::kDeviceMinimumVersionAueMessage,
                                      &eol_admin_message);
  response->SetStringPath("eolAdminMessage", eol_admin_message);
}

void ManagementUIHandler::AddProxyServerPrivacyDisclosure(
    base::Value* response) const {
  bool showProxyDisclosure = false;
  chromeos::NetworkHandler* network_handler = chromeos::NetworkHandler::Get();
  base::Value proxy_settings(base::Value::Type::DICTIONARY);
  // |ui_proxy_config_service| may be missing in tests. If the device is offline
  // (no network connected) the |DefaultNetwork| is null.
  if (chromeos::NetworkHandler::HasUiProxyConfigService() &&
      network_handler->network_state_handler()->DefaultNetwork()) {
    // Check if proxy is enforced by user policy, a forced install extension or
    // ONC policies. This will only read managed settings.
    chromeos::NetworkHandler::GetUiProxyConfigService()
        ->MergeEnforcedProxyConfig(
            network_handler->network_state_handler()->DefaultNetwork()->guid(),
            &proxy_settings);
  }
  if (!proxy_settings.DictEmpty()) {
    // Proxies can be specified by web server url, via a PAC script or via the
    // web proxy auto-discovery protocol. Chrome also supports the "direct"
    // mode, in which no proxy is used.
    base::Value* proxy_specification_mode = proxy_settings.FindPath(
        {::onc::network_config::kType, ::onc::kAugmentationActiveSetting});
    showProxyDisclosure =
        proxy_specification_mode &&
        proxy_specification_mode->GetString() != ::onc::proxy::kDirect;
  }
  response->SetBoolPath("showProxyServerPrivacyDisclosure",
                        showProxyDisclosure);
}
#endif

base::Value ManagementUIHandler::GetContextualManagedData(Profile* profile) {
  base::Value response(base::Value::Type::DICTIONARY);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string enterprise_manager = GetDeviceManager();
  if (enterprise_manager.empty())
    enterprise_manager = GetAccountManager(profile);
  AddUpdateRequiredEolInfo(&response);
  AddProxyServerPrivacyDisclosure(&response);
#else
  std::string enterprise_manager = GetAccountManager(profile);

  response.SetStringPath(
      "browserManagementNotice",
      l10n_util::GetStringFUTF16(
          managed_() ? IDS_MANAGEMENT_BROWSER_NOTICE
                     : IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
          base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
#endif

  if (enterprise_manager.empty()) {
    response.SetStringPath(
        "extensionReportingTitle",
        l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
    response.SetStringPath(
        "managedWebsitesSubtitle",
        l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    response.SetStringPath(
        "pageSubtitle", l10n_util::GetStringUTF16(
                            managed_() ? IDS_MANAGEMENT_SUBTITLE
                                       : IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
#else
    const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
    response.SetStringPath(
        "pageSubtitle",
        managed_()
            ? l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED,
                                         l10n_util::GetStringUTF16(device_type))
            : l10n_util::GetStringFUTF16(
                  IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                  l10n_util::GetStringUTF16(device_type)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  } else {
    response.SetStringPath(
        "extensionReportingTitle",
        l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                   base::UTF8ToUTF16(enterprise_manager)));
    response.SetStringPath("managedWebsitesSubtitle",
                           l10n_util::GetStringFUTF16(
                               IDS_MANAGEMENT_MANAGED_WEBSITES_BY_EXPLANATION,
                               base::UTF8ToUTF16(enterprise_manager)));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    response.SetStringPath(
        "pageSubtitle",
        managed_()
            ? l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                         base::UTF8ToUTF16(enterprise_manager))
            : l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
#else
    const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
    response.SetStringPath(
        "pageSubtitle",
        managed_()
            ? l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                         l10n_util::GetStringUTF16(device_type),
                                         base::UTF8ToUTF16(enterprise_manager))
            : l10n_util::GetStringFUTF16(
                  IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                  l10n_util::GetStringUTF16(device_type)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  }
  response.SetBoolPath("managed", managed_());
  GetManagementStatus(profile, &response);
  AsyncUpdateLogo();
  if (!fetched_image_.empty())
    response.SetPath(kCustomerLogo, base::Value(fetched_image_));
  return response;
}

base::Value ManagementUIHandler::GetThreatProtectionInfo(
    Profile* profile) const {
  base::Value info(base::Value::Type::LIST);
  const policy::PolicyService* policy_service = GetPolicyService();
  const auto& chrome_policies = policy_service->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));

  auto* on_file_attached =
      chrome_policies.GetValue(policy::key::kOnFileAttachedEnterpriseConnector);
  if (on_file_attached && on_file_attached->is_list() &&
      !on_file_attached->GetList().empty()) {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementOnFileAttachedEvent);
    value.SetStringKey("permission", kManagementOnFileAttachedVisibleData);
    info.Append(std::move(value));
  }

  auto* on_file_downloaded = chrome_policies.GetValue(
      policy::key::kOnFileDownloadedEnterpriseConnector);
  if (on_file_downloaded && on_file_downloaded->is_list() &&
      !on_file_downloaded->GetList().empty()) {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementOnFileDownloadedEvent);
    value.SetStringKey("permission", kManagementOnFileDownloadedVisibleData);
    info.Append(std::move(value));
  }

  auto* on_bulk_data_entry = chrome_policies.GetValue(
      policy::key::kOnBulkDataEntryEnterpriseConnector);
  if (on_bulk_data_entry && on_bulk_data_entry->is_list() &&
      !on_bulk_data_entry->GetList().empty()) {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementOnBulkDataEntryEvent);
    value.SetStringKey("permission", kManagementOnBulkDataEntryVisibleData);
    info.Append(std::move(value));
  }

  auto* on_security_event = chrome_policies.GetValue(
      policy::key::kOnSecurityEventEnterpriseConnector);
  if (on_security_event && on_security_event->is_list() &&
      !on_security_event->GetList().empty()) {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementEnterpriseReportingEvent);
    value.SetStringKey("permission", kManagementEnterpriseReportingVisibleData);
    info.Append(std::move(value));
  }

  auto* on_page_visited_event =
      chrome_policies.GetValue(policy::key::kEnterpriseRealTimeUrlCheckMode);
  if (on_page_visited_event && on_page_visited_event->is_int() &&
      on_page_visited_event->GetInt() !=
          safe_browsing::REAL_TIME_CHECK_DISABLED) {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementOnPageVisitedEvent);
    value.SetStringKey("permission", kManagementOnPageVisitedVisibleData);
    info.Append(std::move(value));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string enterprise_manager = GetDeviceManager();
  if (enterprise_manager.empty())
    enterprise_manager = GetAccountManager(profile);
#else
  std::string enterprise_manager = GetAccountManager(profile);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  base::Value result(base::Value::Type::DICTIONARY);
  result.SetStringKey("description",
                      enterprise_manager.empty()
                          ? l10n_util::GetStringUTF16(
                                IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION)
                          : l10n_util::GetStringFUTF16(
                                IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION_BY,
                                base::UTF8ToUTF16(enterprise_manager)));
  result.SetKey("info", std::move(info));
  return result;
}

base::Value ManagementUIHandler::GetManagedWebsitesInfo(
    Profile* profile) const {
  base::Value managed_websites(base::Value::Type::LIST);
  auto* managed_configuration =
      ManagedConfigurationAPIFactory::GetForProfile(profile);

  if (!managed_configuration)
    return managed_websites;

  for (const auto& entry : managed_configuration->GetManagedOrigins()) {
    managed_websites.Append(entry.Serialize());
  }

  return managed_websites;
}

policy::PolicyService* ManagementUIHandler::GetPolicyService() const {
  return Profile::FromWebUI(web_ui())
      ->GetProfilePolicyConnector()
      ->policy_service();
}

void ManagementUIHandler::AsyncUpdateLogo() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  const auto url = connector->GetCustomerLogoURL();
  if (!url.empty() && GURL(url) != logo_url_) {
    icon_fetcher_ = std::make_unique<BitmapFetcher>(
        GURL(url), this, GetManagementUICustomerLogoAnnotation());
    icon_fetcher_->Init(std::string(), net::ReferrerPolicy::NEVER_CLEAR,
                        network::mojom::CredentialsMode::kOmit);
    auto* profile = Profile::FromWebUI(web_ui());
    icon_fetcher_->Start(
        content::BrowserContext::GetDefaultStoragePartition(profile)
            ->GetURLLoaderFactoryForBrowserProcess()
            .get());
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ManagementUIHandler::OnFetchComplete(const GURL& url,
                                          const SkBitmap* bitmap) {
  if (!bitmap)
    return;
  fetched_image_ = webui::GetBitmapDataUrl(*bitmap);
  logo_url_ = url;
  // Fire listener to reload managed data.
  FireWebUIListener("managed_data_changed");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AddStatusOverviewManagedDeviceAndAccount(
    base::Value* status,
    bool device_managed,
    bool account_managed,
    const std::string& device_manager,
    const std::string& account_manager) {
  if (device_managed && account_managed &&
      (account_manager.empty() || account_manager == device_manager)) {
    status->SetKey(kOverview, base::Value(l10n_util::GetStringFUTF16(
                                  IDS_MANAGEMENT_DEVICE_AND_ACCOUNT_MANAGED_BY,
                                  base::UTF8ToUTF16(device_manager))));

    return;
  }

  if (account_managed && !account_manager.empty()) {
    status->SetKey(kOverview, base::Value(l10n_util::GetStringFUTF16(
                                  IDS_MANAGEMENT_ACCOUNT_MANAGED_BY,
                                  base::UTF8ToUTF16(account_manager))));
  }

  if (account_managed && device_managed && !account_manager.empty() &&
      account_manager != device_manager) {
    status->SetKey(kOverview,
                   base::Value(l10n_util::GetStringFUTF16(
                       IDS_MANAGEMENT_DEVICE_MANAGED_BY_ACCOUNT_MANAGED_BY,
                       base::UTF8ToUTF16(device_manager),
                       base::UTF8ToUTF16(account_manager))));
  }
}

const std::string ManagementUIHandler::GetDeviceManager() const {
  std::string device_domain;
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (device_managed_)
    device_domain = connector->GetEnterpriseDomainManager();
  if (device_domain.empty() && connector->IsActiveDirectoryManaged())
    device_domain = connector->GetRealm();
  return device_domain;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void ManagementUIHandler::GetManagementStatus(Profile* profile,
                                              base::Value* status) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  status->SetKey(kDeviceManagedInfo, base::Value());
  status->SetKey(kAccountManagedInfo, base::Value());
  status->SetKey(kOverview, base::Value());
  if (!managed_()) {
    status->SetKey(kOverview, base::Value(l10n_util::GetStringUTF16(
                                  IDS_MANAGEMENT_DEVICE_NOT_MANAGED)));
    return;
  }
  std::string account_manager = GetAccountManager(profile);
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  auto* primary_profile =
      primary_user
          ? chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user)
          : nullptr;
  const bool primary_user_managed =
      primary_profile ? IsProfileManaged(primary_profile) : false;

  if (primary_user_managed)
    account_manager = GetAccountManager(primary_profile);

  std::string device_manager = GetDeviceManager();

  AddStatusOverviewManagedDeviceAndAccount(
      status, device_managed_, account_managed_ || primary_user_managed,
      device_manager, account_manager);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ManagementUIHandler::HandleGetExtensions(const base::ListValue* args) {
  AllowJavascript();
  // List of all enabled extensions
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
          ->enabled_extensions();

  base::Value powerful_extensions = GetPowerfulExtensions(extensions);

  // The number of extensions to be reported in chrome://management with
  // powerful permissions.
  base::UmaHistogramCounts1000(kPowerfulExtensionsCountHistogram,
                               powerful_extensions.GetList().size());

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            powerful_extensions);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ManagementUIHandler::HandleGetLocalTrustRootsInfo(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  base::Value trust_roots_configured(false);
  AllowJavascript();

  policy::PolicyCertService* policy_service =
      policy::PolicyCertServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (policy_service && policy_service->has_policy_certificates())
    trust_roots_configured = base::Value(true);

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            trust_roots_configured);
}

void ManagementUIHandler::HandleGetDeviceReportingInfo(
    const base::ListValue* args) {
  base::Value report_sources(base::Value::Type::LIST);
  AllowJavascript();

  const policy::DeviceCloudPolicyManagerChromeOS* manager =
      GetDeviceCloudPolicyManager();
  policy::StatusUploader* uploader = nullptr;
  policy::SystemLogUploader* syslog_uploader = nullptr;
  policy::StatusCollector* collector = nullptr;
  if (manager) {
    uploader = manager->GetStatusUploader();
    syslog_uploader = manager->GetSystemLogUploader();
    if (uploader)
      collector = uploader->status_collector();
  }
  AddDeviceReportingInfo(&report_sources, collector, syslog_uploader,
                         Profile::FromWebUI(web_ui()));

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            report_sources);
}

void ManagementUIHandler::HandleGetPluginVmDataCollectionStatus(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  base::Value plugin_vm_data_collection_enabled(
      Profile::FromWebUI(web_ui())->GetPrefs()->GetBoolean(
          plugin_vm::prefs::kPluginVmDataCollectionAllowed));
  AllowJavascript();
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            plugin_vm_data_collection_enabled);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void ManagementUIHandler::HandleGetContextualManagedData(
    const base::ListValue* args) {
  AllowJavascript();
  auto result = GetContextualManagedData(Profile::FromWebUI(web_ui()));
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            std::move(result));
}

void ManagementUIHandler::HandleGetThreatProtectionInfo(
    const base::ListValue* args) {
  AllowJavascript();
  ResolveJavascriptCallback(
      args->GetList()[0] /* callback_id */,
      GetThreatProtectionInfo(Profile::FromWebUI(web_ui())));
}

void ManagementUIHandler::HandleGetManagedWebsites(
    const base::ListValue* args) {
  AllowJavascript();

  ResolveJavascriptCallback(
      args->GetList()[0] /* callback_id */,
      GetManagedWebsitesInfo(Profile::FromWebUI(web_ui())));
}

void ManagementUIHandler::HandleInitBrowserReportingInfo(
    const base::ListValue* args) {
  base::Value report_sources(base::Value::Type::LIST);
  AllowJavascript();
  AddReportingInfo(&report_sources);
  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            report_sources);
}

void ManagementUIHandler::NotifyBrowserReportingInfoUpdated() {
  base::Value report_sources(base::Value::Type::LIST);
  AddReportingInfo(&report_sources);
  FireWebUIListener("browser-reporting-info-updated", report_sources);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ManagementUIHandler::NotifyPluginVmDataCollectionUpdated() {
  FireWebUIListener(
      "plugin-vm-data-collection-updated",
      base::Value(Profile::FromWebUI(web_ui())->GetPrefs()->GetBoolean(
          plugin_vm::prefs::kPluginVmDataCollectionAllowed)));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void ManagementUIHandler::NotifyThreatProtectionInfoUpdated() {
  FireWebUIListener("threat-protection-info-updated",
                    GetThreatProtectionInfo(Profile::FromWebUI(web_ui())));
}

void ManagementUIHandler::OnExtensionLoaded(
    content::BrowserContext* /*browser_context*/,
    const extensions::Extension* extension) {
  if (reporting_extension_ids_.find(extension->id()) !=
      reporting_extension_ids_.end()) {
    NotifyBrowserReportingInfoUpdated();
  }
}

void ManagementUIHandler::OnExtensionUnloaded(
    content::BrowserContext* /*browser_context*/,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason /*reason*/) {
  if (reporting_extension_ids_.find(extension->id()) !=
      reporting_extension_ids_.end()) {
    NotifyBrowserReportingInfoUpdated();
  }
}

void ManagementUIHandler::UpdateManagedState() {
  auto* profile = Profile::FromWebUI(web_ui());
  bool managed_state_changed = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  managed_state_changed |= account_managed_ != IsProfileManaged(profile);
  managed_state_changed |= device_managed_ != IsDeviceManaged();
  account_managed_ = IsProfileManaged(profile);
  device_managed_ = IsDeviceManaged();
#else
  managed_state_changed |=
      account_managed_ != (IsProfileManaged(profile) || IsBrowserManaged());
  account_managed_ = IsProfileManaged(profile) || IsBrowserManaged();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (managed_state_changed)
    FireWebUIListener("managed_data_changed");
}

void ManagementUIHandler::OnPolicyUpdated(
    const policy::PolicyNamespace& /*ns*/,
    const policy::PolicyMap& /*previous*/,
    const policy::PolicyMap& /*current*/) {
  UpdateManagedState();
  NotifyBrowserReportingInfoUpdated();
  NotifyThreatProtectionInfoUpdated();
}

void ManagementUIHandler::AddObservers() {
  if (has_observers_)
    return;

  has_observers_ = true;

  auto* profile = Profile::FromWebUI(web_ui());

  extensions::ExtensionRegistry::Get(profile)->AddObserver(this);

  auto* policy_service = GetPolicyService();
  policy_service->AddObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);

  pref_registrar_.Init(profile->GetPrefs());

  pref_registrar_.Add(
      prefs::kSupervisedUserId,
      base::BindRepeating(&ManagementUIHandler::UpdateManagedState,
                          base::Unretained(this)));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  pref_registrar_.Add(
      plugin_vm::prefs::kPluginVmDataCollectionAllowed,
      base::BindRepeating(
          &ManagementUIHandler::NotifyPluginVmDataCollectionUpdated,
          base::Unretained(this)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ManagementUIHandler::RemoveObservers() {
  if (!has_observers_)
    return;

  has_observers_ = false;

  extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
      ->RemoveObserver(this);

  policy::PolicyService* policy_service = Profile::FromWebUI(web_ui())
                                              ->GetProfilePolicyConnector()
                                              ->policy_service();
  policy_service->RemoveObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);

  pref_registrar_.RemoveAll();
}
