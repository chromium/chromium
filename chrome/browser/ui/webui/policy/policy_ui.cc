
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/policy_ui.h"

#include <memory>

#include "base/check.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/schema_registry_service.h"
#include "chrome/browser/policy/value_provider/chrome_policies_value_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/policy/policy_ui_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/grit/policy_resources.h"
#include "components/grit/policy_resources_map.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_loader_common.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_utils.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/strings/grit/components_strings.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_handler_helper.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/user_agent.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"

// LINT.IfChange

namespace {

// Returns the operating system information to be displayed on
// chrome://policy/logs page.
std::string GetOsInfo() {
#if BUILDFLAG(IS_ANDROID)
  // The base format for the OS version and build.
  constexpr char kOSVersionAndBuildFormat[] = "Android %s %s";
  return base::StringPrintf(
      kOSVersionAndBuildFormat,
      (base::SysInfo::OperatingSystemVersion()).c_str(),
      (content::GetAndroidOSInfo(content::IncludeAndroidBuildNumber::Include,
                                 content::IncludeAndroidModel::Include))
          .c_str());
#else
  return base::StringPrintf("%s %s",
                            base::SysInfo::OperatingSystemName().c_str(),
                            base::SysInfo::OperatingSystemVersion().c_str());
#endif  //  BUILDFLAG (IS_ANDROID)
}

// Returns the version information to be displayed on the chrome://policy/logs
// page.
base::Value::Dict GetVersionInfo() {
  base::Value::Dict version_info;

  version_info.Set("revision", version_info::GetLastChange());
  version_info.Set("version", version_info::GetVersionNumber());
  version_info.Set("deviceOs", GetOsInfo());
  version_info.Set("variations", version_ui::GetVariationsList());

  return version_info;
}

void CreateAndAddPolicyUIHtmlSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIPolicyHost);
  PolicyUIHandler::AddCommonLocalizedStringsToSource(source);

  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order).
      {"copyPoliciesJSON", IDS_COPY_POLICIES_JSON},
      {"exportPoliciesJSON", IDS_EXPORT_POLICIES_JSON},
      {"filterPlaceholder", IDS_POLICY_FILTER_PLACEHOLDER},
      {"hideExpandedStatus", IDS_POLICY_HIDE_EXPANDED_STATUS},
      {"isAffiliatedYes", IDS_POLICY_IS_AFFILIATED_YES},
      {"isAffiliatedNo", IDS_POLICY_IS_AFFILIATED_NO},
      {"labelAssetId", IDS_POLICY_LABEL_ASSET_ID},
      {"labelClientId", IDS_POLICY_LABEL_CLIENT_ID},
      {"labelDirectoryApiId", IDS_POLICY_LABEL_DIRECTORY_API_ID},
      {"labelError", IDS_POLICY_LABEL_ERROR},
      {"labelWarning", IDS_POLICY_HEADER_WARNING},
      {"labelGaiaId", IDS_POLICY_LABEL_GAIA_ID},
      {"labelIsAffiliated", IDS_POLICY_LABEL_IS_AFFILIATED},
      {"labelLastCloudReportSentTimestamp",
       IDS_POLICY_LABEL_LAST_CLOUD_REPORT_SENT_TIMESTAMP},
      {"labelLocation", IDS_POLICY_LABEL_LOCATION},
      {"labelMachineEnrollmentDomain",
       IDS_POLICY_LABEL_MACHINE_ENROLLMENT_DOMAIN},
      {"labelMachineEnrollmentMachineName",
       IDS_POLICY_LABEL_MACHINE_ENROLLMENT_MACHINE_NAME},
      {"labelMachineEnrollmentToken",
       IDS_POLICY_LABEL_MACHINE_ENROLLMENT_TOKEN},
      {"labelMachineEntrollmentDeviceId",
       IDS_POLICY_LABEL_MACHINE_ENROLLMENT_DEVICE_ID},
      {"labelIsOffHoursActive", IDS_POLICY_LABEL_IS_OFFHOURS_ACTIVE},
      {"labelPoliciesPush", IDS_POLICY_LABEL_PUSH_POLICIES},
      {"labelPrecedence", IDS_POLICY_LABEL_PRECEDENCE},
      {"labelProfileId", IDS_POLICY_LABEL_PROFILE_ID},
      {"labelRefreshInterval", IDS_POLICY_LABEL_REFRESH_INTERVAL},
      {"labelStatus", IDS_POLICY_LABEL_STATUS},
      {"labelTimeSinceLastFetchAttempt",
       IDS_POLICY_LABEL_TIME_SINCE_LAST_FETCH_ATTEMPT},
      {"labelTimeSinceLastRefresh", IDS_POLICY_LABEL_TIME_SINCE_LAST_REFRESH},
      {"labelUsername", IDS_POLICY_LABEL_USERNAME},
      {"labelManagedBy", IDS_POLICY_LABEL_MANAGED_BY},
      {"labelVersion", IDS_POLICY_LABEL_VERSION},
      {"moreActions", IDS_POLICY_MORE_ACTIONS},
      {"noPoliciesSet", IDS_POLICY_NO_POLICIES_SET},
      {"offHoursActive", IDS_POLICY_OFFHOURS_ACTIVE},
      {"offHoursNotActive", IDS_POLICY_OFFHOURS_NOT_ACTIVE},
      {"policyCopyValue", IDS_POLICY_COPY_VALUE},
      {"policiesPushOff", IDS_POLICY_PUSH_POLICIES_OFF},
      {"policiesPushOn", IDS_POLICY_PUSH_POLICIES_ON},
      {"policyLearnMore", IDS_POLICY_LEARN_MORE},
      {"reloadPolicies", IDS_POLICY_RELOAD_POLICIES},
      {"showExpandedStatus", IDS_POLICY_SHOW_EXPANDED_STATUS},
      {"showLess", IDS_POLICY_SHOW_LESS},
      {"showMore", IDS_POLICY_SHOW_MORE},
      {"showUnset", IDS_POLICY_SHOW_UNSET},
      {"signinProfile", IDS_POLICY_SIGNIN_PROFILE},
      {"status", IDS_POLICY_STATUS},
      {"statusErrorManagedNoPolicy", IDS_POLICY_STATUS_ERROR_MANAGED_NO_POLICY},
      {"statusFlexOrgNoPolicy", IDS_POLICY_STATUS_FLEX_ORG_NO_POLICY},
      {"statusDevice", IDS_POLICY_STATUS_DEVICE},
      {"statusMachine", IDS_POLICY_STATUS_MACHINE},
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
      {"statusUpdater", IDS_POLICY_STATUS_UPDATER},
#endif
      {"statusUser", IDS_POLICY_STATUS_USER},
#if !BUILDFLAG(IS_CHROMEOS)
      {"uploadReport", IDS_UPLOAD_REPORT},
#endif  // !BUILDFLAG(IS_CHROMEOS)
      {"viewLogs", IDS_VIEW_POLICY_LOGS},
#if !BUILDFLAG(IS_ANDROID)
      {"promotionBannerTitle", IDS_POLICY_BANNER_PROMOTION_TITLE},
      {"promotionBannerDesc", IDS_POLICY_BANNER_PROMOTION_DESC},
      {"promotionBannerBtn", IDS_POLICY_BANNER_PROMOTION_BTN},
#endif
  };
  source->AddLocalizedStrings(kStrings);

  // Localized strings for chrome://policy/logs.
  static constexpr webui::LocalizedString kPolicyLogsStrings[] = {
      {"browserName", IDS_PRODUCT_NAME},
      {"exportLogsJSON", IDS_EXPORT_POLICY_LOGS_JSON},
      {"logsTitle", IDS_POLICY_LOGS_TITLE},
      {"os", IDS_VERSION_UI_OS},
      {"refreshLogs", IDS_REFRESH_POLICY_LOGS},
      {"revision", IDS_VERSION_UI_REVISION},
      {"versionInfoLabel", IDS_VERSION_INFO},
      {"variations", IDS_VERSION_UI_VARIATIONS},
  };
  source->AddLocalizedStrings(kPolicyLogsStrings);

  std::string variations_json_value;
  base::JSONWriter::Write(GetVersionInfo(), &variations_json_value);

  source->AddString("versionInfo", variations_json_value);

  source->AddResourcePath("logs/", IDR_POLICY_LOGS_POLICY_LOGS_HTML);
  source->AddResourcePath("logs", IDR_POLICY_LOGS_POLICY_LOGS_HTML);

  const bool allow_policy_test_page = PolicyUI::ShouldLoadTestPage(profile);
  if (allow_policy_test_page) {
    // Localized strings for chrome://policy/test.
    static constexpr webui::LocalizedString kPolicyTestStrings[] = {
        {"testTitle", IDS_POLICY_TEST_TITLE},
        {"testRestart", IDS_POLICY_TEST_RESTART_AND_APPLY},
        {"testApply", IDS_POLICY_TEST_APPLY},
        {"testImport", IDS_POLICY_TEST_IMPORT},
        {"testDesc", IDS_POLICY_TEST_DESC},
        {"testRevertAppliedPolicies", IDS_POLICY_TEST_REVERT},
        {"testClearPolicies", IDS_CLEAR},
        {"testTableNamespace", IDS_POLICY_HEADER_NAMESPACE},
        {"testTableName", IDS_POLICY_HEADER_NAME},
        {"testTableSource", IDS_POLICY_HEADER_SOURCE},
        {"testTableScope", IDS_POLICY_TEST_TABLE_SCOPE},
        {"testTableLevel", IDS_POLICY_HEADER_LEVEL},
        {"testTableValue", IDS_POLICY_LABEL_VALUE},
        {"testTableRemove", IDS_REMOVE},
        {"testAdd", IDS_POLICY_TEST_ADD},
        {"testNameSelect", IDS_POLICY_SELECT_NAME},
        {"testTablePreset", IDS_POLICY_TEST_TABLE_PRESET},
        {"testTablePresetCustom", IDS_POLICY_TEST_PRESET_CUSTOM},
        {"testTablePresetLocalMachine", IDS_POLICY_TEST_PRESET_LOCAL_MACHINE},
        {"testTablePresetCloudAccount", IDS_POLICY_TEST_PRESET_CLOUD_ACCOUNT},
        {"testUserAffiliated", IDS_POLICY_TEST_USER_AFFILIATED},
    };

    source->AddLocalizedStrings(kPolicyTestStrings);
    source->AddResourcePath("test/", IDR_POLICY_TEST_POLICY_TEST_HTML);
    source->AddResourcePath("test", IDR_POLICY_TEST_POLICY_TEST_HTML);

    std::string schema;
    JSONStringValueSerializer serializer(&schema);
    serializer.Serialize(PolicyUI::GetSchema(profile));
    source->AddString("initialSchema", schema);

    // Strings for policy levels, scopes and sources.
    static constexpr webui::LocalizedString kPolicyTestTypes[] = {
        {"scopeUser", IDS_POLICY_SCOPE_USER},
        {"scopeDevice", IDS_POLICY_SCOPE_DEVICE},
        {"levelRecommended", IDS_POLICY_LEVEL_RECOMMENDED},
        {"levelMandatory", IDS_POLICY_LEVEL_MANDATORY},
        {"sourceEnterpriseDefault", IDS_POLICY_SOURCE_ENTERPRISE_DEFAULT},
        {"sourceCommandLine", IDS_POLICY_SOURCE_COMMAND_LINE},
        {"sourceCloud", IDS_POLICY_SOURCE_CLOUD},
        {"sourceActiveDirectory", IDS_POLICY_SOURCE_ACTIVE_DIRECTORY},
        {"sourcePlatform", IDS_POLICY_SOURCE_PLATFORM},
        {"sourceMerged", IDS_POLICY_SOURCE_MERGED},
        {"sourceCloudFromAsh", IDS_POLICY_SOURCE_CLOUD_FROM_ASH},
        {"sourceRestrictedManagedGuestSessionOverride",
         IDS_POLICY_SOURCE_RESTRICTED_MANAGED_GUEST_SESSION_OVERRIDE},
    };

    source->AddLocalizedStrings(kPolicyTestTypes);
  }

  source->AddString("acceptedPaths",
                    allow_policy_test_page ? "/|/test|/logs" : "/|/logs");
  webui::SetupWebUIDataSource(
      source, base::make_span(kPolicyResources, kPolicyResourcesSize),
      IDR_POLICY_POLICY_HTML);

  webui::EnableTrustedTypesCSP(source);
}

}  // namespace

PolicyUI::PolicyUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<PolicyUIHandler>());
  CreateAndAddPolicyUIHtmlSource(Profile::FromWebUI(web_ui));
}

PolicyUI::~PolicyUI() = default;

// static
void PolicyUI::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(policy::policy_prefs::kPolicyTestPageEnabled,
                                true);
  registry->RegisterBooleanPref(
      policy::policy_prefs::kHasDismissedPolicyPagePromotionBanner, false);
}

// static
bool PolicyUI::ShouldLoadTestPage(Profile* profile) {
  // Test page should only load if testing is enabled.
  if (!policy::utils::IsPolicyTestingEnabled(profile->GetPrefs(),
                                             chrome::GetChannel())) {
    return false;
  }
  // The test page is not allowed if the profile is cloud managed unless
  // we are already using the test policies.
  if (policy::ManagementServiceFactory::GetForProfile(profile)
          ->HasManagementAuthority(
              policy::EnterpriseManagementAuthority::CLOUD) &&
      !profile->GetProfilePolicyConnector()->IsUsingLocalTestPolicyProvider()) {
    return false;
  }
  return true;
}

// static
base::Value PolicyUI::GetSchema(Profile* profile) {
  if (!profile->GetPolicySchemaRegistryService()) {
    return base::Value();
  }

  policy::SchemaRegistry* registry =
      profile->GetPolicySchemaRegistryService()->registry();
  static const policy::PolicyDomain kDomains[] = {
      policy::POLICY_DOMAIN_CHROME,
      policy::POLICY_DOMAIN_EXTENSIONS,
  };
  // Build a dictionary like this:
  // {
  //   "chrome": {
  //     "PolicyOne": "number",
  //     "PolicyTwo": "string",
  //     ...
  //   },
  //   "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa": {
  //     "PolicyOne": "number",
  //     ...
  //   },
  //   ...
  // }
  base::Value::Dict dict;
  for (const auto domain : kDomains) {
    const policy::ComponentMap* components =
        registry->schema_map()->GetComponents(domain);
    if (!components) {
      continue;
    }
    for (const auto& [component_id, schema] : *components) {
      DCHECK_EQ(schema.type(), base::Value::Type::DICT);
      base::Value::List policy_names;
      auto it = schema.GetPropertiesIterator();
      for (; !it.IsAtEnd(); it.Advance()) {
        if (it.schema().IsSensitiveValue() ||
            policy::IsPolicyNameSensitive(it.key())) {
          continue;
        }
        policy_names.Append(it.key());
      }
      // Use "chrome" instead of the empty string for the Chrome namespace,
      // for better debuggability. Use the extension ID for other namespaces.
      dict.Set(domain == policy::POLICY_DOMAIN_CHROME ? "chrome" : component_id,
               policy::utils::GetPolicyNameToTypeMapping(policy_names, schema));
    }
  }
  return base::Value(std::move(dict));
}

// LINT.ThenChange(//ios/chrome/browser/webui/ui_bundled/policy/policy_ui.mm)
