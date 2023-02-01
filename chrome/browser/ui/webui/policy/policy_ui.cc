// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/policy/policy_ui.h"

#include <memory>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/policy/policy_ui_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/grit/policy_resources.h"
#include "components/grit/policy_resources_map.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/version_info/version_info.h"
#include "components/version_ui/version_handler_helper.h"
#include "content/public/common/user_agent.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

#if BUILDFLAG(IS_ANDROID)
// Returns the operating system information to be displayed on
// chrome://policy/logs page.
std::string GetOsInfo() {
  // The base format for the OS version and build
  constexpr char kOSVersionAndBuildFormat[] = "Android %s %s";
  return base::StringPrintf(
      kOSVersionAndBuildFormat,
      (base::SysInfo::OperatingSystemVersion()).c_str(),
      (content::GetAndroidOSInfo(content::IncludeAndroidBuildNumber::Include,
                                 content::IncludeAndroidModel::Include))
          .c_str());
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
#endif

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
    {"labelMachineEnrollmentToken", IDS_POLICY_LABEL_MACHINE_ENROLLMENT_TOKEN},
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

#if BUILDFLAG(IS_ANDROID)
  source->AddBoolean(
      "loggingEnabled",
      policy::PolicyLogger::GetInstance()->IsPolicyLoggingEnabled());

  if (policy::PolicyLogger::GetInstance()->IsPolicyLoggingEnabled()) {
    std::string variations_json_value;
    base::JSONWriter::Write(GetVersionInfo(), &variations_json_value);

    source->AddString("versionInfo", variations_json_value);
  }

  source->AddResourcePath("logs/", IDR_POLICY_LOGS_POLICY_LOGS_HTML);
  source->AddResourcePath("logs", IDR_POLICY_LOGS_POLICY_LOGS_HTML);
#endif  // BUILDFLAG(IS_ANDROID)

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
