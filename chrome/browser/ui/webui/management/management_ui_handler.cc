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

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_icon_source.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/device_api/managed_configuration_api.h"
#include "chrome/browser/device_api/managed_configuration_api_factory.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/webui/management/management_ui_constants.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "management_ui_handler.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/referrer_policy.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"
#include "components/device_signals/core/browser/user_permission_service.h"  // nogncheck
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

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

enum class ReportingType {
  kDevice,
  kExtensions,
  kSecurity,
  kUser,
  kUserActivity,
  kLegacyTech,
  kUrl,
};

namespace {

#if !BUILDFLAG(IS_CHROMEOS)

bool IsBrowserManaged() {
  return g_browser_process->browser_policy_connector()
      ->HasMachineLevelPolicies();
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

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
    case ReportingType::kLegacyTech:
      return kReportingTypeLegacyTech;
    case ReportingType::kUrl:
      return kReportingTypeUrl;
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

}  // namespace

ManagementUIHandler::ManagementUIHandler(Profile* profile) {
  reporting_extension_ids_ = {kOnPremReportingExtensionStableId,
                              kOnPremReportingExtensionBetaId};
  UpdateAccountManagedState(profile);
#if !BUILDFLAG(IS_CHROMEOS)
  UpdateBrowserManagedState();
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

ManagementUIHandler::~ManagementUIHandler() {
  DisallowJavascript();
}

#if !BUILDFLAG(IS_CHROMEOS)
std::unique_ptr<ManagementUIHandler> ManagementUIHandler::Create(
    Profile* profile) {
  return std::make_unique<ManagementUIHandler>(profile);
}
#endif  //  !BUILDFLAG(IS_CHROMEOS)

void ManagementUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getContextualManagedData",
      base::BindRepeating(&ManagementUIHandler::HandleGetContextualManagedData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getExtensions",
      base::BindRepeating(&ManagementUIHandler::HandleGetExtensions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getThreatProtectionInfo",
      base::BindRepeating(&ManagementUIHandler::HandleGetThreatProtectionInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getManagedWebsites",
      base::BindRepeating(&ManagementUIHandler::HandleGetManagedWebsites,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getApplications",
      base::BindRepeating(&ManagementUIHandler::HandleGetApplications,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initBrowserReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleInitBrowserReportingInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "initProfileReportingInfo",
      base::BindRepeating(&ManagementUIHandler::HandleInitProfileReportingInfo,
                          base::Unretained(this)));
}

void ManagementUIHandler::OnJavascriptAllowed() {
  AddObservers();
}

void ManagementUIHandler::OnJavascriptDisallowed() {
  RemoveObservers();
}

void ManagementUIHandler::AddReportingInfo(base::Value::List* report_sources,
                                           bool is_browser) {
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

  const bool cloud_reporting_policy_enabled =
      g_browser_process->local_state()->GetBoolean(
          enterprise_reporting::kCloudReportingEnabled);
  const bool cloud_legacy_tech_report_enabled =
      !Profile::FromWebUI(web_ui())
           ->GetPrefs()
           ->GetList(enterprise_reporting::kCloudLegacyTechReportAllowlist)
           .empty();
  const bool cloud_profile_reporting_policy_enabled =
      Profile::FromWebUI(web_ui())->GetPrefs()->GetBoolean(
          enterprise_reporting::kCloudProfileReportingEnabled);

  const bool real_time_url_check_connector_enabled =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()))
          ->GetAppliedRealTimeUrlCheck() !=
      enterprise_connectors::REAL_TIME_CHECK_DISABLED;

  if (cloud_legacy_tech_report_enabled) {
    Profile::FromWebUI(web_ui())->GetPrefs()->GetList(
        enterprise_reporting::kCloudLegacyTechReportAllowlist)[0];
  }

  const struct {
    const char* reporting_extension_policy_key;
    const char* message;
    const ReportingType reporting_type;
    const bool cloud_reporting_enabled;
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
      {kPolicyKeyReportVisitedUrlData, kManagementExtensionReportVisitedUrl,
       ReportingType::kUrl, real_time_url_check_connector_enabled},
      {kPolicyKeyReportUserBrowsingData, kManagementLegacyTechReport,
       ReportingType::kLegacyTech, cloud_legacy_tech_report_enabled}};

  if (is_browser) {
    std::unordered_set<const char*> enabled_messages;
    for (auto& report_definition : report_definitions) {
      if (report_definition.cloud_reporting_enabled) {
        enabled_messages.insert(report_definition.message);
      } else if (report_definition.reporting_extension_policy_key) {
        for (const policy::PolicyMap* policy_map : policy_maps) {
          const base::Value* policy_value = policy_map->GetValue(
              report_definition.reporting_extension_policy_key,
              base::Value::Type::BOOLEAN);
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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    auto device_signal_data = GetDeviceSignalGrantedMessage();
    if (!device_signal_data.empty()) {
      report_sources->Append(std::move(device_signal_data));
    }
#endif
  } else {
    if (cloud_reporting_policy_enabled ||
        !cloud_profile_reporting_policy_enabled) {
      return;
    }

    const std::string messages[] = {
        kProfileReportingOverview, kProfileReportingUsername,
        kProfileReportingBrowser, kProfileReportingExtension,
        kProfileReportingPolicy};
    for (const auto& message : messages) {
      base::Value::Dict data;
      data.Set("messageId", message);
      report_sources->Append(std::move(data));
    }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    auto device_signal_data = GetDeviceSignalGrantedMessage();
    if (!device_signal_data.empty()) {
      report_sources->Append(std::move(device_signal_data));
    }
#endif
    base::Value::Dict learn_more_data;
    learn_more_data.Set("messageId", kProfileReportingLearnMore);
    report_sources->Append(std::move(learn_more_data));
  }
}

base::Value::Dict ManagementUIHandler::GetContextualManagedData(
    Profile* profile) {
  base::Value::Dict response;
#if !BUILDFLAG(IS_CHROMEOS)
  int message_id = IDS_MANAGEMENT_NOT_MANAGED_NOTICE;
  if (browser_managed_) {
    message_id = IDS_MANAGEMENT_BROWSER_NOTICE;
  } else if (account_managed_) {
    message_id = IDS_MANAGEMENT_PROFILE_NOTICE;
  }

  response.Set("browserManagementNotice",
               l10n_util::GetStringFUTF16(
                   message_id, chrome::kManagedUiLearnMoreUrl,
                   base::EscapeForHTML(l10n_util::GetStringUTF16(
                       IDS_MANAGEMENT_LEARN_MORE_ACCCESSIBILITY_TEXT))));
  response.Set("pageSubtitle", chrome::GetManagementPageSubtitle(profile));

  response.Set("extensionReportingSubtitle",
               l10n_util::GetStringUTF16(IDS_MANAGEMENT_EXTENSIONS_INSTALLED));
  response.Set(
      "applicationReportingSubtitle",
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_APPLICATIONS_INSTALLED));
  response.Set(
      "managedWebsitesSubtitle",
      l10n_util::GetStringUTF16(IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION));

  response.Set("managed", managed());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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
#if BUILDFLAG(IS_CHROMEOS)
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
      enterprise_connectors::REAL_TIME_CHECK_DISABLED) {
    AddThreatProtectionPermission(kManagementOnPageVisitedEvent,
                                  kManagementOnPageVisitedVisibleData, &info);
  }

  if (connectors_service
          ->GetReportingSettings(
              enterprise_connectors::ReportingConnector::SECURITY_EVENT)
          .has_value() &&
      connectors_service
              ->GetReportingSettings(
                  enterprise_connectors::ReportingConnector::SECURITY_EVENT)
              ->enabled_opt_in_events.count(
                  enterprise_connectors::kExtensionTelemetryEvent) > 0) {
    AddThreatProtectionPermission(kManagementOnExtensionTelemetryEvent,
                                  kManagementOnExtensionTelemetryVisibleData,
                                  &info);
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (is_get_all_screens_media_allowed_for_any_origin_) {
    AddThreatProtectionPermission(kManagementScreenCaptureEvent,
                                  kManagementScreenCaptureData, &info);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

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

base::Value::List ManagementUIHandler::GetApplicationsInfo(
    Profile* profile) const {
  base::Value::List applications;

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  // Only display web apps for the profile that contains them e.g. Lacros
  // primary profile when Lacros is enabled.
  if (provider == nullptr) {
    return applications;
  }

  auto& registrar = provider->registrar_unsafe();

  for (const webapps::AppId& app_id : registrar.GetAppIds()) {
    base::Value::List permission_messages;
    // Display RunOnOsLogin if it is set to autostart by admin policy.
    web_app::ValueWithPolicy<web_app::RunOnOsLoginMode> policy =
        registrar.GetAppRunOnOsLoginMode(app_id);
    if (!policy.user_controllable &&
        web_app::IsRunOnOsLoginModeEnabledForAutostart(policy.value)) {
      permission_messages.Append(l10n_util::GetStringUTF16(
          IDS_MANAGEMENT_APPLICATIONS_RUN_ON_OS_LOGIN));
    }

    if (!permission_messages.empty()) {
      base::Value::Dict app_info;
      app_info.Set("name", registrar.GetAppShortName(app_id));
      // We try to match the same icon size as used for the extensions
      GURL icon = apps::AppIconSource::GetIconURL(
          app_id, extension_misc::EXTENSION_ICON_SMALLISH);
      app_info.Set("icon", icon.spec());
      app_info.Set("permissions", std::move(permission_messages));
      applications.Append(std::move(app_info));
    }
  }

  return applications;
}

policy::PolicyService* ManagementUIHandler::GetPolicyService() {
  return Profile::FromWebUI(web_ui())
      ->GetProfilePolicyConnector()
      ->policy_service();
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
device_signals::UserPermissionService*
ManagementUIHandler::GetUserPermissionService() {
  return enterprise_signals::UserPermissionServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));
}

base::Value::Dict ManagementUIHandler::GetDeviceSignalGrantedMessage() {
  // Insert the device signals consent disclosure at the end of browser
  // reporting section.
  auto* user_permission_service = GetUserPermissionService();
  if (user_permission_service && user_permission_service->CanCollectSignals() ==
                                     device_signals::UserPermission::kGranted) {
    base::Value::Dict data;
    data.Set("messageId", kManagementDeviceSignalsDisclosure);
    data.Set("reportingType", GetReportingTypeValue(ReportingType::kDevice));
    return data;
  }
  return base::Value::Dict();
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

bool ManagementUIHandler::managed() const {
  return account_managed() || browser_managed_;
}

void ManagementUIHandler::RegisterPrefChange(
    PrefChangeRegistrar& pref_registrar) {
  pref_registrar_.Add(
      prefs::kSupervisedUserId,
      base::BindRepeating(&ManagementUIHandler::UpdateManagedState,
                          base::Unretained(this)));
}

void ManagementUIHandler::UpdateManagedState() {
#if !BUILDFLAG(IS_CHROMEOS)
  bool is_account_updated =
      UpdateAccountManagedState(Profile::FromWebUI(web_ui()));
  bool is_browser_updated = UpdateBrowserManagedState();
  if (is_account_updated || is_browser_updated) {
    FireWebUIListener("managed_data_changed");
  }
#endif
}

bool ManagementUIHandler::UpdateAccountManagedState(Profile* profile) {
  if (!profile) {
    CHECK_IS_TEST();
    return false;
  }
  bool new_managed = IsProfileManaged(profile);
  bool is_updated = (new_managed != account_managed_);
  account_managed_ = new_managed;
  return is_updated;
}

#if !BUILDFLAG(IS_CHROMEOS)
bool ManagementUIHandler::UpdateBrowserManagedState() {
  bool new_managed = IsBrowserManaged();
  bool is_updated = (new_managed != browser_managed_);
  browser_managed_ = new_managed;
  return is_updated;
}
#endif

std::string ManagementUIHandler::GetAccountManager(Profile* profile) const {
  std::optional<std::string> manager =
      chrome::GetAccountManagerIdentity(profile);
  if (!manager &&
      base::FeatureList::IsEnabled(features::kFlexOrgManagementDisclosure)) {
    manager = chrome::GetDeviceManagerIdentity();
  }

  return manager.value_or(std::string());
}

bool ManagementUIHandler::IsProfileManaged(Profile* profile) const {
  return profile->GetProfilePolicyConnector()->IsManaged();
}

void ManagementUIHandler::HandleGetExtensions(const base::Value::List& args) {
  AllowJavascript();
  // List of all enabled extensions
  const extensions::ExtensionSet& extensions =
      extensions::ExtensionRegistry::Get(Profile::FromWebUI(web_ui()))
          ->enabled_extensions();

  ResolveJavascriptCallback(args[0] /* callback_id */,
                            GetPowerfulExtensions(extensions));
}

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

void ManagementUIHandler::HandleGetApplications(const base::Value::List& args) {
  AllowJavascript();

  ResolveJavascriptCallback(args[0] /* callback_id */,
                            GetApplicationsInfo(Profile::FromWebUI(web_ui())));
}

void ManagementUIHandler::HandleInitBrowserReportingInfo(
    const base::Value::List& args) {
  base::Value::List report_sources;
  AllowJavascript();
  AddReportingInfo(&report_sources, /*is_browser=*/true);
  ResolveJavascriptCallback(args[0] /* callback_id */, report_sources);
}

void ManagementUIHandler::HandleInitProfileReportingInfo(
    const base::Value::List& args) {
  base::Value::List report_sources;
  AllowJavascript();
  AddReportingInfo(&report_sources, /*is_browser=*/false);
  ResolveJavascriptCallback(args[0] /* callback_id */, report_sources);
}

void ManagementUIHandler::NotifyBrowserReportingInfoUpdated() {
  base::Value::List report_sources;
  AddReportingInfo(&report_sources, /*is_browser=*/true);
  FireWebUIListener("browser-reporting-info-updated", report_sources);
}

void ManagementUIHandler::NotifyProfileReportingInfoUpdated() {
  base::Value::List report_sources;
  AddReportingInfo(&report_sources, /*is_browser=*/false);
  FireWebUIListener("profile-reporting-info-updated", report_sources);
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

void ManagementUIHandler::OnPolicyUpdated(
    const policy::PolicyNamespace& /*ns*/,
    const policy::PolicyMap& /*previous*/,
    const policy::PolicyMap& /*current*/) {
  UpdateManagedState();
  NotifyBrowserReportingInfoUpdated();
  NotifyProfileReportingInfoUpdated();
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

  RegisterPrefChange(pref_registrar_);
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
