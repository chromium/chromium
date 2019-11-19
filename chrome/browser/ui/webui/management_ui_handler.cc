// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui_handler.h"

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/profiles/profile_util.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/status_collector/device_status_collector.h"
#include "chrome/browser/chromeos/policy/status_uploader.h"
#include "chrome/browser/chromeos/policy/system_log_uploader.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/ui/webui/management_ui_handler_chromeos.h"
#include "chrome/grit/chromium_strings.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/chromeos/devicetype_utils.h"
#endif  // defined(OS_CHROMEOS)

#include "chrome/browser/extensions/extension_util.h"
#include "chrome/common/extensions/permissions/chrome_permission_message_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
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
const char kCloudReportingExtensionId[] = "oempjldejiginopiohodkdoklcjklbaa";
const char kPolicyKeyReportMachineIdData[] = "report_machine_id_data";
const char kPolicyKeyReportUserIdData[] = "report_user_id_data";
const char kPolicyKeyReportVersionData[] = "report_version_data";
const char kPolicyKeyReportPolicyData[] = "report_policy_data";
const char kPolicyKeyReportExtensionsData[] = "report_extensions_data";
const char kPolicyKeyReportSafeBrowsingData[] = "report_safe_browsing_data";
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
const char kManagementExtensionReportSafeBrowsingWarnings[] =
    "managementExtensionReportSafeBrowsingWarnings";
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
const char kManagementEnterpriseReportingName[] =
    "managementEnterpriseReportingName";
const char kManagementEnterpriseReportingPermissions[] =
    "managementEnterpriseReportingPermissions";

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

#if defined(OS_CHROMEOS)
const char kManagementLogUploadEnabled[] = "managementLogUploadEnabled";
const char kManagementReportActivityTimes[] = "managementReportActivityTimes";
const char kManagementReportHardwareStatus[] = "managementReportHardwareStatus";
const char kManagementReportNetworkInterfaces[] =
    "managementReportNetworkInterfaces";
const char kManagementReportUsers[] = "managementReportUsers";
const char kManagementPrinting[] = "managementPrinting";
const char kManagementCrostini[] = "managementCrostini";
const char kAccountManagedInfo[] = "accountManagedInfo";
const char kDeviceManagedInfo[] = "deviceManagedInfo";
const char kOverview[] = "overview";
#endif  // defined(OS_CHROMEOS)

const char kCustomerLogo[] = "customerLogo";

namespace {

bool IsProfileManaged(Profile* profile) {
  return profile->GetProfilePolicyConnector()->IsManaged();
}

#if defined(OS_CHROMEOS)
bool IsDeviceManaged() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  return connector->IsEnterpriseManaged();
}
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_CHROMEOS)
bool IsBrowserManaged() {
  return g_browser_process->browser_policy_connector()
      ->HasMachineLevelPolicies();
}
#endif  // !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)

enum class DeviceReportingType {
  kSupervisedUser,
  kDeviceActivity,
  kDeviceStatistics,
  kDevice,
  kLogs,
  kPrint,
  kCrostini
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
    case DeviceReportingType::kLogs:
      return "logs";
    case DeviceReportingType::kPrint:
      return "print";
    case DeviceReportingType::kCrostini:
      return "crostini";
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

void AddDeviceReportingInfo(base::Value* report_sources, Profile* profile) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  // Only check for report status in managed environment.
  if (!connector->IsEnterpriseManaged())
    return;

  policy::DeviceCloudPolicyManagerChromeOS* manager =
      connector->GetDeviceCloudPolicyManager();

  if (!manager)
    return;

  const policy::StatusCollector* collector =
      manager->GetStatusUploader()->status_collector();

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
  if (manager->GetSystemLogUploader()->upload_enabled()) {
    AddDeviceReportingElement(report_sources, kManagementLogUploadEnabled,
                              DeviceReportingType::kLogs);
  }

  if (profile->GetPrefs()->GetBoolean(
          prefs::kPrintingSendUsernameAndFilenameEnabled)) {
    AddDeviceReportingElement(report_sources, kManagementPrinting,
                              DeviceReportingType::kPrint);
  }

  if (profile->GetPrefs()->GetBoolean(
          crostini::prefs::kReportCrostiniUsageEnabled)) {
    AddDeviceReportingElement(report_sources, kManagementCrostini,
                              DeviceReportingType::kCrostini);
  }
}
#endif  // defined(OS_CHROMEOS)

std::vector<base::Value> GetPermissionsForExtension(
    scoped_refptr<const extensions::Extension> extension) {
  std::vector<base::Value> permission_messages;
  // Only consider force installed extensions
  if (!extensions::Manifest::IsPolicyLocation(extension->location()))
    return permission_messages;

  extensions::PermissionIDSet permissions =
      extensions::PermissionMessageProvider::Get()->GetAllPermissionIDs(
          extension->permissions_data()->active_permissions(),
          extension->GetType());

  const extensions::PermissionMessages messages =
      extensions::PermissionMessageProvider::Get()
          ->GetPowerfulPermissionMessages(permissions);

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

// TODO(raleksandov) Move to util class or smth similar.
// static
std::string ManagementUIHandler::GetAccountDomain(Profile* profile) {
  auto username = profile->GetProfileUserName();
  size_t email_separator_pos = username.find('@');
  bool is_email = email_separator_pos != std::string::npos &&
                  email_separator_pos < username.length() - 1;

  if (!is_email)
    return std::string();

  const std::string domain = gaia::ExtractDomainName(std::move(username));

  return (domain == "gmail.com" || domain == "googlemail.com") ? std::string()
                                                               : domain;
}

ManagementUIHandler::ManagementUIHandler() {
  reporting_extension_ids_ = {kOnPremReportingExtensionStableId,
                              kOnPremReportingExtensionBetaId,
                              kCloudReportingExtensionId};
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

#if defined(OS_CHROMEOS)
  handler->account_managed_ = IsProfileManaged(profile);
  handler->device_managed_ = IsDeviceManaged();
#else
  handler->account_managed_ = IsProfileManaged(profile) || IsBrowserManaged();
#endif  // defined(OS_CHROMEOS)

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
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "getLocalTrustRootsInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetLocalTrustRootsInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDeviceReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetDeviceReportingInfo,
                          base::Unretained(this)));
#endif  // defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "getThreatProtectionInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetThreatProtectionInfo,
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
  const extensions::Extension* cloud_reporting_extension =
      GetEnabledExtension(kCloudReportingExtensionId);

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

  const bool cloud_reporting_extension_installed =
      cloud_reporting_extension != nullptr;
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
       ReportingType::kDevice,
       cloud_reporting_extension_installed || cloud_reporting_policy_enabled},
      {kPolicyKeyReportMachineIdData,
       kManagementExtensionReportMachineNameAddress, ReportingType::kDevice,
       false},
      {kPolicyKeyReportVersionData, kManagementExtensionReportVersion,
       ReportingType::kDevice,
       cloud_reporting_extension_installed || cloud_reporting_policy_enabled},
      {kPolicyKeyReportSystemTelemetryData, kManagementExtensionReportPerfCrash,
       ReportingType::kDevice, false},
      {kPolicyKeyReportUserIdData, kManagementExtensionReportUsername,
       ReportingType::kUser,
       cloud_reporting_extension_installed || cloud_reporting_policy_enabled},
      {kPolicyKeyReportSafeBrowsingData,
       kManagementExtensionReportSafeBrowsingWarnings, ReportingType::kSecurity,
       cloud_reporting_extension_installed},
      {kPolicyKeyReportExtensionsData,
       kManagementExtensionReportExtensionsPlugin, ReportingType::kExtensions,
       cloud_reporting_extension_installed || cloud_reporting_policy_enabled},
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

base::DictionaryValue ManagementUIHandler::GetContextualManagedData(
    Profile* profile) {
  base::DictionaryValue response;
#if defined(OS_CHROMEOS)
  std::string management_domain = GetDeviceDomain();
  if (management_domain.empty())
    management_domain = GetAccountDomain(profile);
#else
  std::string management_domain = GetAccountDomain(profile);

  response.SetString("browserManagementNotice",
                     l10n_util::GetStringFUTF16(
                         managed_() ? IDS_MANAGEMENT_BROWSER_NOTICE
                                    : IDS_MANAGEMENT_NOT_MANAGED_NOTICE,
                         base::UTF8ToUTF16(chrome::kManagedUiLearnMoreUrl)));
#endif

  if (management_domain.empty()) {
    response.SetString(
        "extensionReportingTitle",
        l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));

#if !defined(OS_CHROMEOS)
    response.SetString("pageSubtitle",
                       l10n_util::GetStringUTF16(
                           managed_() ? IDS_MANAGEMENT_SUBTITLE
                                      : IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
#else
    const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
    response.SetString(
        "pageSubtitle",
        managed_()
            ? l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED,
                                         l10n_util::GetStringUTF16(device_type))
            : l10n_util::GetStringFUTF16(
                  IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                  l10n_util::GetStringUTF16(device_type)));
#endif  // !defined(OS_CHROMEOS)

  } else {
    response.SetString(
        "extensionReportingTitle",
        l10n_util::GetStringFUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED_BY,
                                   base::UTF8ToUTF16(management_domain)));

#if !defined(OS_CHROMEOS)
    response.SetString(
        "pageSubtitle",
        managed_()
            ? l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                         base::UTF8ToUTF16(management_domain))
            : l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE));
#else
    const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
    response.SetString(
        "pageSubtitle",
        managed_()
            ? l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                         l10n_util::GetStringUTF16(device_type),
                                         base::UTF8ToUTF16(management_domain))
            : l10n_util::GetStringFUTF16(
                  IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                  l10n_util::GetStringUTF16(device_type)));
#endif  // !defined(OS_CHROMEOS)
  }
  response.SetBoolean("managed", managed_());
  GetManagementStatus(profile, &response);
  AsyncUpdateLogo();
  if (!fetched_image_.empty())
    response.SetKey(kCustomerLogo, base::Value(fetched_image_));
  return response;
}

base::Value ManagementUIHandler::GetThreatProtectionInfo(
    Profile* profile) const {
  base::Value info(base::Value::Type::LIST);
  const policy::PolicyService* policy_service = GetPolicyService();
  const auto& chrome_policies = policy_service->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  // CheckContentCompliance is a int-enum policy. The accepted values are
  // defined in the enum CheckContentComplianceValues.
  auto* check_content_compliance_value =
      chrome_policies.GetValue(policy::key::kCheckContentCompliance);
  if (check_content_compliance_value &&
      check_content_compliance_value->GetInt() > safe_browsing::CHECK_NONE &&
      check_content_compliance_value->GetInt() <=
          safe_browsing::CHECK_CONTENT_COMPLIANCE_MAX) {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementDataLossPreventionName);
    value.SetStringKey("permission", kManagementDataLossPreventionPermissions);
    info.Append(std::move(value));
  }

  // SendFilesForMalwareCheck is a int-enum policy. The accepted values are
  // defined in the enum SendFilesForMalwareCheckValues.
  auto* send_files_for_malware_check_value =
      chrome_policies.GetValue(policy::key::kSendFilesForMalwareCheck);
  if (send_files_for_malware_check_value &&
      send_files_for_malware_check_value->GetInt() >
          safe_browsing::DO_NOT_SCAN &&
      send_files_for_malware_check_value->GetInt() <=
          safe_browsing::SEND_FILES_FOR_MALWARE_CHECK_MAX) {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementMalwareScanningName);
    value.SetStringKey("permission", kManagementMalwareScanningPermissions);
    info.Append(std::move(value));
  }

  auto* unsafe_event_reporting_value =
      chrome_policies.GetValue(policy::key::kUnsafeEventsReportingEnabled);
  if (unsafe_event_reporting_value && unsafe_event_reporting_value->GetBool()) {
    base::Value value(base::Value::Type::DICTIONARY);
    value.SetStringKey("title", kManagementEnterpriseReportingName);
    value.SetStringKey("permission", kManagementEnterpriseReportingPermissions);
    info.Append(std::move(value));
  }

#if defined(OS_CHROMEOS)
  std::string management_domain = GetDeviceDomain();
  if (management_domain.empty())
    management_domain = GetAccountDomain(profile);
#else
  std::string management_domain = GetAccountDomain(profile);
#endif  // defined(OS_CHROMEOS)

  base::Value result(base::Value::Type::DICTIONARY);
  result.SetStringKey("description",
                      management_domain.empty()
                          ? l10n_util::GetStringUTF16(
                                IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION)
                          : l10n_util::GetStringFUTF16(
                                IDS_MANAGEMENT_THREAT_PROTECTION_DESCRIPTION_BY,
                                base::UTF8ToUTF16(management_domain)));
  result.SetKey("info", std::move(info));
  return result;
}

policy::PolicyService* ManagementUIHandler::GetPolicyService() const {
  return Profile::FromWebUI(web_ui())
      ->GetProfilePolicyConnector()
      ->policy_service();
}

const extensions::Extension* ManagementUIHandler::GetEnabledExtension(
    const std::string& extensionId) const {
  return extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
      ->GetExtensionById(kCloudReportingExtensionId,
                         extensions::ExtensionRegistry::ENABLED);
}

void ManagementUIHandler::AsyncUpdateLogo() {
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  const auto url = connector->GetCustomerLogoURL();
  if (!url.empty() && GURL(url) != logo_url_) {
    icon_fetcher_ = std::make_unique<BitmapFetcher>(
        GURL(url), this, GetManagementUICustomerLogoAnnotation());
    icon_fetcher_->Init(std::string(), net::URLRequest::NEVER_CLEAR_REFERRER,
                        network::mojom::CredentialsMode::kOmit);
    auto* profile = Profile::FromWebUI(web_ui());
    icon_fetcher_->Start(
        content::BrowserContext::GetDefaultStoragePartition(profile)
            ->GetURLLoaderFactoryForBrowserProcess()
            .get());
  }
#endif  // defined(OS_CHROMEOS)
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

#if defined(OS_CHROMEOS)
void AddStatusOverviewManagedDeviceAndAccount(
    base::Value* status,
    bool device_managed,
    bool account_managed,
    const std::string& device_domain,
    const std::string& account_domain) {
  if (device_managed && account_managed &&
      (account_domain.empty() || account_domain == device_domain)) {
    status->SetKey(kOverview, base::Value(l10n_util::GetStringFUTF16(
                                  IDS_MANAGEMENT_DEVICE_AND_ACCOUNT_MANAGED_BY,
                                  base::UTF8ToUTF16(device_domain))));

    return;
  }

  if (account_managed && !account_domain.empty()) {
    status->SetKey(kOverview, base::Value(l10n_util::GetStringFUTF16(
                                  IDS_MANAGEMENT_ACCOUNT_MANAGED_BY,
                                  base::UTF8ToUTF16(account_domain))));
  }

  if (account_managed && device_managed && !account_domain.empty() &&
      account_domain != device_domain) {
    status->SetKey(kOverview,
                   base::Value(l10n_util::GetStringFUTF16(
                       IDS_MANAGEMENT_DEVICE_MANAGED_BY_ACCOUNT_MANAGED_BY,
                       base::UTF8ToUTF16(device_domain),
                       base::UTF8ToUTF16(account_domain))));
  }
}

const std::string ManagementUIHandler::GetDeviceDomain() const {
  std::string device_domain;
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (device_managed_)
    device_domain = connector->GetEnterpriseDisplayDomain();
  if (device_domain.empty() && connector->IsActiveDirectoryManaged())
    device_domain = connector->GetRealm();
  return device_domain;
}

#endif  // defined(OS_CHROMEOS)

void ManagementUIHandler::GetManagementStatus(Profile* profile,
                                              base::Value* status) const {
#if defined(OS_CHROMEOS)
  status->SetKey(kDeviceManagedInfo, base::Value());
  status->SetKey(kAccountManagedInfo, base::Value());
  status->SetKey(kOverview, base::Value());
  if (!managed_()) {
    status->SetKey(kOverview, base::Value(l10n_util::GetStringUTF16(
                                  IDS_MANAGEMENT_DEVICE_NOT_MANAGED)));
    return;
  }
  std::string account_domain = GetAccountDomain(profile);
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  auto* primary_profile =
      primary_user
          ? chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user)
          : nullptr;
  const bool primary_user_managed =
      primary_profile ? IsProfileManaged(primary_profile) : false;

  if (primary_user_managed)
    account_domain = GetAccountDomain(primary_profile);

  std::string device_domain = GetDeviceDomain();

  AddStatusOverviewManagedDeviceAndAccount(
      status, device_managed_, account_managed_ || primary_user_managed,
      device_domain, account_domain);
#endif  // defined(OS_CHROMEOS)
}

void ManagementUIHandler::HandleGetExtensions(const base::ListValue* args) {
  AllowJavascript();
  // List of all enabled extensions
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
          ->enabled_extensions();

  base::Value powerful_extensions = GetPowerfulExtensions(extensions);

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            powerful_extensions);
}

#if defined(OS_CHROMEOS)
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

  AddDeviceReportingInfo(&report_sources, Profile::FromWebUI(web_ui()));

  ResolveJavascriptCallback(args->GetList()[0] /* callback_id */,
                            report_sources);
}
#endif  // defined(OS_CHROMEOS)

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
#if defined(OS_CHROMEOS)
  managed_state_changed |= account_managed_ != IsProfileManaged(profile);
  managed_state_changed |= device_managed_ != IsDeviceManaged();
  account_managed_ = IsProfileManaged(profile);
  device_managed_ = IsDeviceManaged();
#else
  managed_state_changed |=
      account_managed_ != (IsProfileManaged(profile) || IsBrowserManaged());
  account_managed_ = IsProfileManaged(profile) || IsBrowserManaged();
#endif  // defined(OS_CHROMEOS)

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
