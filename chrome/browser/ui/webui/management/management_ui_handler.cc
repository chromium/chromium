// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui_handler.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/management_utils.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/managed_ui.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/policy/networking/policy_cert_service.h"
#include "chrome/browser/policy/networking/policy_cert_service_factory.h"
#include "chrome/browser/ui/webui/webui_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_handler.h"
#include "chrome/browser/ash/policy/status_collector/device_status_collector.h"
#include "chrome/browser/ash/policy/status_collector/status_collector.h"
#include "chrome/browser/ash/policy/uploading/status_uploader.h"
#include "chrome/browser/ash/policy/uploading/system_log_uploader.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/net/stub_resolver_config_reader.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/webui/management/management_ui_handler_chromeos.h"
#include "chrome/grit/chromium_strings.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
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
const char kManagementOnPrintEvent[] = "managementOnPrintEvent";
const char kManagementOnPrintVisibleData[] = "managementOnPrintVisibleData";

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

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
const char kManagementScreenCaptureEvent[] = "managementScreenCaptureEvent";
const char kManagementScreenCaptureData[] = "managementScreenCaptureData";
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
const char kManagementLogUploadEnabled[] = "managementLogUploadEnabled";
const char kManagementReportActivityTimes[] = "managementReportActivityTimes";
const char kManagementReportDeviceAudioStatus[] =
    "managementReportDeviceAudioStatus";
const char kManagementReportDeviceGraphicsStatus[] =
    "managementReportDeviceGraphicsStatus";
const char kManagementReportDevicePeripherals[] =
    "managementReportDevicePeripherals";
const char kManagementReportNetworkData[] = "managementReportNetworkData";
const char kManagementReportHardwareData[] = "managementReportHardwareData";
const char kManagementReportUsers[] = "managementReportUsers";
const char kManagementReportCrashReports[] = "managementReportCrashReports";
const char kManagementReportAppInfoAndActivity[] =
    "managementReportAppInfoAndActivity";
const char kManagementReportExtensions[] = "managementReportExtensions";
const char kManagementReportAndroidApplications[] =
    "managementReportAndroidApplications";
const char kManagementReportPrintJobs[] = "managementReportPrintJobs";
const char kManagementReportLoginLogout[] = "managementReportLoginLogout";
const char kManagementReportCRDSessions[] = "managementReportCRDSessions";
const char kManagementReportDlpEvents[] = "managementReportDlpEvents";
const char kManagementOnFileTransferEvent[] = "managementOnFileTransferEvent";
const char kManagementOnFileTransferVisibleData[] =
    "managementOnFileTransferVisibleData";
const char kManagementPrinting[] = "managementPrinting";
const char kManagementCrostini[] = "managementCrostini";
const char kManagementCrostiniContainerConfiguration[] =
    "managementCrostiniContainerConfiguration";
const char kAccountManagedInfo[] = "accountManagedInfo";
const char kDeviceManagedInfo[] = "deviceManagedInfo";
const char kOverview[] = "overview";
#endif  // BUILDFLAG(IS_CHROMEOS)

const char kCustomerLogo[] = "customerLogo";

const char kPowerfulExtensionsCountHistogram[] = "Extensions.PowerfulCount";

namespace {

bool IsProfileManaged(Profile* profile) {
  return profile->GetProfilePolicyConnector()->IsManaged();
}

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

bool IsDeviceManaged() {
  return policy::IsDeviceEnterpriseManaged();
}

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
};

#else

bool IsBrowserManaged() {
  return g_browser_process->browser_policy_connector()
      ->HasMachineLevelPolicies();
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)

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
    default:
      NOTREACHED() << "Unknown device reporting type";
      return "device";
  }
}

void AddDeviceReportingElement(base::Value::List* report_sources,
                               const std::string& message_id,
                               const DeviceReportingType& type) {
  base::Value::Dict data;
  data.Set("messageId", message_id);
  data.Set("reportingType", ToJSDeviceReportingType(type));
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
  if (collector->IsReportingAppInfoAndActivity() || device_report_xdr_events) {
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

  bool report_login_logout = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kReportDeviceLoginLogout,
                                       &report_login_logout);
  if (report_login_logout) {
    AddDeviceReportingElement(report_sources, kManagementReportLoginLogout,
                              DeviceReportingType::kLoginLogout);
  }

  bool report_crd_sessions = false;
  ash::CrosSettings::Get()->GetBoolean(ash::kReportCRDSessions,
                                       &report_crd_sessions);
  if (report_crd_sessions) {
    AddDeviceReportingElement(report_sources, kManagementReportCRDSessions,
                              DeviceReportingType::kCRDSessions);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

base::Value::List GetPermissionsForExtension(
    scoped_refptr<const extensions::Extension> extension) {
  base::Value::List permission_messages;
  // Only consider force installed extensions
  if (!extensions::Manifest::IsPolicyLocation(extension->location())) {
    return permission_messages;
  }

  extensions::PermissionIDSet permissions =
      extensions::PermissionMessageProvider::Get()
          ->GetManagementUIPermissionIDs(
              extension->permissions_data()->active_permissions(),
              extension->GetType());

  const extensions::PermissionMessages messages =
      extensions::PermissionMessageProvider::Get()->GetPermissionMessages(
          permissions);

  for (const auto& message : messages) {
    permission_messages.Append(message.message());
  }

  return permission_messages;
}

base::Value::List GetPowerfulExtensions(
    const extensions::ExtensionSet& extensions) {
  base::Value::List powerful_extensions;

  for (const auto& extension : extensions) {
    base::Value::List permission_messages =
        GetPermissionsForExtension(extension);

    // Only show extension on page if there is at least one permission
    // message to show.
    if (!permission_messages.empty()) {
      base::Value::Dict extension_to_add =
          extensions::util::GetExtensionInfo(extension.get());
      extension_to_add.Set("permissions", std::move(permission_messages));
      powerful_extensions.Append(std::move(extension_to_add));
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

void AddThreatProtectionPermission(const char* title,
                                   const char* permission,
                                   base::Value::List* info) {
  base::Value::Dict value;
  value.Set("title", title);
  value.Set("permission", permission);
  info->Append(std::move(value));
}

std::string GetAccountManager(Profile* profile) {
  absl::optional<std::string> manager =
      chrome::GetAccountManagerIdentity(profile);
  if (!manager &&
      base::FeatureList::IsEnabled(features::kFlexOrgManagementDisclosure)) {
    manager = chrome::GetDeviceManagerIdentity();
  }

  return manager.value_or(std::string());
}

}  // namespace

ManagementUIHandler::ManagementUIHandler() {
  reporting_extension_ids_ = {kOnPremReportingExtensionStableId,
                              kOnPremReportingExtensionBetaId};
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  // Get device report sources.
  if (service->IsAvailable<crosapi::mojom::DeviceSettingsService>() &&
      service->GetInterfaceVersion(
          crosapi::mojom::DeviceSettingsService::Uuid_) >=
          static_cast<int>(crosapi::mojom::DeviceSettingsService::
                               kGetDeviceReportSourcesMinVersion)) {
    service->GetRemote<crosapi::mojom::DeviceSettingsService>()
        ->GetDeviceReportSources(
            base::BindOnce(&ManagementUIHandler::OnGotDeviceReportSources,
                           weak_factory_.GetWeakPtr()));
  }
#endif
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

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  handler->account_managed_ = IsProfileManaged(profile);
  handler->device_managed_ = IsDeviceManaged();
#else
  handler->account_managed_ = IsProfileManaged(profile) || IsBrowserManaged();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

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
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
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

void ManagementUIHandler::AddReportingInfo(base::Value::List* report_sources) {
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
          .GetValue(policy::key::kCloudReportingEnabled,
                    base::Value::Type::BOOLEAN);
  const bool cloud_reporting_policy_enabled =
      cloud_reporting_policy_value && cloud_reporting_policy_value->GetBool();

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
        const base::Value* policy_value = policy_map->GetValue(
            report_definition.policy_key, base::Value::Type::BOOLEAN);
        if (policy_value && policy_value->GetBool()) {
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

    base::Value::Dict data;
    data.Set("messageId", report_definition.message);
    data.Set("reportingType",
             GetReportingTypeValue(report_definition.reporting_type));
    report_sources->Append(std::move(data));
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
const policy::DeviceCloudPolicyManagerAsh*
ManagementUIHandler::GetDeviceCloudPolicyManager() const {
  // Only check for report status in managed environment.
  if (!device_managed_) {
    return nullptr;
  }

  const policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  return connector->GetDeviceCloudPolicyManager();
}

bool ManagementUIHandler::IsUpdateRequiredEol() const {
  const policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::MinimumVersionPolicyHandler* handler =
      connector->GetMinimumVersionPolicyHandler();
  return handler && handler->ShouldShowUpdateRequiredEolBanner();
}

void ManagementUIHandler::AddUpdateRequiredEolInfo(
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

void ManagementUIHandler::AddMonitoredNetworkPrivacyDisclosure(
    base::Value::Dict* response) const {
  bool showMonitoredNetworkDisclosure = false;

  // Check for secure DNS templates with identifiers.
  showMonitoredNetworkDisclosure =
      SystemNetworkContextManager::GetStubResolverConfigReader()
          ->GetDohWithIdentifiersDisplayServers()
          .has_value();
  if (showMonitoredNetworkDisclosure) {
    response->Set("showMonitoredNetworkPrivacyDisclosure",
                  showMonitoredNetworkDisclosure);
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

// static
base::Value::List ManagementUIHandler::GetDeviceReportingInfo(
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
void ManagementUIHandler::AddDlpDeviceReportingElementForTesting(
    base::Value::List* report_sources,
    const std::string& message_id) {
  AddDeviceReportingElement(report_sources, message_id,
                            DeviceReportingType::kDlpEvents);
}

// static
void ManagementUIHandler::AddDeviceReportingInfoForTesting(
    base::Value::List* report_sources,
    const policy::StatusCollector* collector,
    const policy::SystemLogUploader* uploader,
    Profile* profile) {
  AddDeviceReportingInfo(report_sources, collector, uploader, profile);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

base::Value::Dict ManagementUIHandler::GetContextualManagedData(
    Profile* profile) {
  base::Value::Dict response;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string enterprise_manager = GetDeviceManager();
  if (enterprise_manager.empty()) {
    enterprise_manager = GetAccountManager(profile);
  }
  AddUpdateRequiredEolInfo(&response);
  AddMonitoredNetworkPrivacyDisclosure(&response);
#else
  std::string enterprise_manager = GetAccountManager(profile);

  response.Set("browserManagementNotice",
               l10n_util::GetStringFUTF16(
                   managed_() ? IDS_MANAGEMENT_BROWSER_NOTICE
                              : IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                   base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl),
                   base::EscapeForHTML(l10n_util::GetStringUTF16(
                       IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
#endif

  if (enterprise_manager.empty()) {
    response.Set(
        "extensionReportingTitle",
        l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
    response.Set(
        "managedWebsitesSubtitle",
        l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    response.Set("pageSubtitle",
                 l10n_util::GetStringUTF16(
                     managed_() ? IDS_MANAGEMENT_SUBTITLE
                                : IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
#else
    const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
    response.Set("pageSubtitle",
                 managed_() ? l10n_util::GetStringFUTF16(
                                  IDS_MANAGEMENT_SUBTITLE_MANAGED,
                                  l10n_util::GetStringUTF16(device_type))
                            : l10n_util::GetStringFUTF16(
                                  IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                  l10n_util::GetStringUTF16(device_type)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  } else {
    response.Set(
        "extensionReportingTitle",
        l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                   base::UTF8ToUTF16(enterprise_manager)));
    response.Set("managedWebsitesSubtitle",
                 l10n_util::GetStringFUTF16(
                     IDS_MANAGEMENT_MANAGED_WEBSITES_BY_EXPLANATION,
                     base::UTF8ToUTF16(enterprise_manager)));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    response.Set(
        "pageSubtitle",
        managed_()
            ? l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                         base::UTF8ToUTF16(enterprise_manager))
            : l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
#else
    const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
    response.Set("pageSubtitle",
                 managed_() ? l10n_util::GetStringFUTF16(
                                  IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                  l10n_util::GetStringUTF16(device_type),
                                  base::UTF8ToUTF16(enterprise_manager))
                            : l10n_util::GetStringFUTF16(
                                  IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                  l10n_util::GetStringUTF16(device_type)));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  }
  response.Set("managed", managed_());
  GetManagementStatus(profile, &response);
  AsyncUpdateLogo();
  if (!fetched_image_.empty()) {
    response.Set(kCustomerLogo, base::Value(fetched_image_));
  }
  return response;
}

base::Value::Dict ManagementUIHandler::GetThreatProtectionInfo(
    Profile* profile) {
  base::Value::List info;

  constexpr struct {
    enterprise_connectors::AnalysisConnector connector;
    const char* title;
    const char* permission;
  } analysis_connector_permissions[] = {
    {enterprise_connectors::FILE_ATTACHED, kManagementOnFileAttachedEvent,
     kManagementOnFileAttachedVisibleData},
    {enterprise_connectors::FILE_DOWNLOADED, kManagementOnFileDownloadedEvent,
     kManagementOnFileDownloadedVisibleData},
    {enterprise_connectors::BULK_DATA_ENTRY, kManagementOnBulkDataEntryEvent,
     kManagementOnBulkDataEntryVisibleData},
    {enterprise_connectors::PRINT, kManagementOnPrintEvent,
     kManagementOnPrintVisibleData},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {enterprise_connectors::FILE_TRANSFER, kManagementOnFileTransferEvent,
     kManagementOnFileTransferVisibleData},
#endif
  };
  auto* connectors_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  for (auto& entry : analysis_connector_permissions) {
    if (!connectors_service->GetAnalysisServiceProviderNames(entry.connector)
             .empty()) {
      AddThreatProtectionPermission(entry.title, entry.permission, &info);
    }
  }

  if (!connectors_service
           ->GetReportingServiceProviderNames(
               enterprise_connectors::ReportingConnector::SECURITY_EVENT)
           .empty()) {
    AddThreatProtectionPermission(kManagementEnterpriseReportingEvent,
                                  kManagementEnterpriseReportingVisibleData,
                                  &info);
  }

  if (connectors_service->GetAppliedRealTimeUrlCheck() !=
      safe_browsing::REAL_TIME_CHECK_DISABLED) {
    AddThreatProtectionPermission(kManagementOnPageVisitedEvent,
                                  kManagementOnPageVisitedVisibleData, &info);
  }

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  if (capture_policy::IsGetDisplayMediaSetSelectAllScreensAllowedForAnySite(
          profile)) {
    AddThreatProtectionPermission(kManagementScreenCaptureEvent,
                                  kManagementScreenCaptureData, &info);
  }
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

  const std::string enterprise_manager =
      connectors_service->GetManagementDomain();

  base::Value::Dict result;
  result.Set("description",
             enterprise_manager.empty()
                 ? l10n_util::GetStringUTF16(
                       IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION)
                 : l10n_util::GetStringFUTF16(
                       IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION_BY,
                       base::UTF8ToUTF16(enterprise_manager)));
  result.Set("info", std::move(info));
  return result;
}

base::Value::List ManagementUIHandler::GetManagedWebsitesInfo(
    Profile* profile) const {
  base::Value::List managed_websites;
  auto* managed_configuration =
      ManagedConfigurationAPIFactory::GetForProfile(profile);

  if (!managed_configuration) {
    return managed_websites;
  }

  for (const auto& entry : managed_configuration->GetManagedOrigins()) {
    managed_websites.Append(entry.Serialize());
  }

  return managed_websites;
}

policy::PolicyService* ManagementUIHandler::GetPolicyService() {
  return Profile::FromWebUI(web_ui())
      ->GetProfilePolicyConnector()
      ->policy_service();
}

void ManagementUIHandler::AsyncUpdateLogo() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ManagementUIHandler::OnFetchComplete(const GURL& url,
                                          const SkBitmap* bitmap) {
  if (!bitmap) {
    return;
  }
  fetched_image_ = webui::GetBitmapDataUrl(*bitmap);
  logo_url_ = url;
  // Fire listener to reload managed data.
  FireWebUIListener("managed_data_changed");
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
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

const std::string ManagementUIHandler::GetDeviceManager() const {
  std::string device_domain;
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (device_managed_) {
    device_domain = connector->GetEnterpriseDomainManager();
  }
  if (device_domain.empty() && connector->IsActiveDirectoryManaged()) {
    device_domain = connector->GetRealm();
  }
  return device_domain;
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void ManagementUIHandler::GetManagementStatus(Profile* profile,
                                              base::Value::Dict* status) const {
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  status->Set(kDeviceManagedInfo, base::Value());
  status->Set(kAccountManagedInfo, base::Value());
  status->Set(kOverview, base::Value());
  if (!managed_()) {
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
      status, device_managed_, account_managed_ || primary_user_managed,
      device_manager, account_manager);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
}

void ManagementUIHandler::HandleGetExtensions(const base::Value::List& args) {
  AllowJavascript();
  // List of all enabled extensions
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
          ->enabled_extensions();

  base::Value::List powerful_extensions = GetPowerfulExtensions(extensions);

  // The number of extensions to be reported in chrome://management with
  // powerful permissions.
  base::UmaHistogramCounts1000(kPowerfulExtensionsCountHistogram,
                               powerful_extensions.size());

  ResolveJavascriptCallback(args[0] /* callback_id */, powerful_extensions);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void ManagementUIHandler::OnGotDeviceReportSources(
    base::Value::List report_sources,
    bool plugin_vm_data_collection_enabled) {
  report_sources_ = std::move(report_sources);
  plugin_vm_data_collection_enabled_ = plugin_vm_data_collection_enabled;
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
void ManagementUIHandler::HandleGetLocalTrustRootsInfo(
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

void ManagementUIHandler::HandleGetDeviceReportingInfo(
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

void ManagementUIHandler::HandleGetPluginVmDataCollectionStatus(
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

void ManagementUIHandler::HandleGetContextualManagedData(
    const base::Value::List& args) {
  AllowJavascript();
  auto result = GetContextualManagedData(Profile::FromWebUI(web_ui()));
  ResolveJavascriptCallback(args[0] /* callback_id */, result);
}

void ManagementUIHandler::HandleGetThreatProtectionInfo(
    const base::Value::List& args) {
  AllowJavascript();
  ResolveJavascriptCallback(
      args[0] /* callback_id */,
      GetThreatProtectionInfo(Profile::FromWebUI(web_ui())));
}

void ManagementUIHandler::HandleGetManagedWebsites(
    const base::Value::List& args) {
  AllowJavascript();

  ResolveJavascriptCallback(
      args[0] /* callback_id */,
      GetManagedWebsitesInfo(Profile::FromWebUI(web_ui())));
}

void ManagementUIHandler::HandleInitBrowserReportingInfo(
    const base::Value::List& args) {
  base::Value::List report_sources;
  AllowJavascript();
  AddReportingInfo(&report_sources);
  ResolveJavascriptCallback(args[0] /* callback_id */, report_sources);
}

void ManagementUIHandler::NotifyBrowserReportingInfoUpdated() {
  base::Value::List report_sources;
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
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  managed_state_changed |= account_managed_ != IsProfileManaged(profile);
  managed_state_changed |= device_managed_ != IsDeviceManaged();
  account_managed_ = IsProfileManaged(profile);
  device_managed_ = IsDeviceManaged();
#else
  managed_state_changed |=
      account_managed_ != (IsProfileManaged(profile) || IsBrowserManaged());
  account_managed_ = IsProfileManaged(profile) || IsBrowserManaged();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

  if (managed_state_changed) {
    FireWebUIListener("managed_data_changed");
  }
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
  if (has_observers_) {
    return;
  }

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
  if (!has_observers_) {
    return;
  }

  has_observers_ = false;

  extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
      ->RemoveObserver(this);

  policy::PolicyService* policy_service = Profile::FromWebUI(web_ui())
                                              ->GetProfilePolicyConnector()
                                              ->policy_service();
  policy_service->RemoveObserver(policy::POLICY_DOMAIN_EXTENSIONS, this);

  pref_registrar_.RemoveAll();
}
