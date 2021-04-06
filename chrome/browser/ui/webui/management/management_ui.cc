// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management/management_ui.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/chromeos/devicetype_utils.h"
#else  // BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

content::WebUIDataSource* CreateManagementUIHtmlSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIManagementHost);

  source->DisableTrustedTypesCSP();

  source->AddString("pageSubtitle",
                    ManagementUI::GetManagementPageSubtitle(profile));

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {"learnMore", IDS_LEARN_MORE},
    {"localTrustRoots", IDS_MANAGEMENT_LOCAL_TRUST_ROOTS},
    {"managementTrustRootsConfigured", IDS_MANAGEMENT_TRUST_ROOTS_CONFIGURED},
    {"deviceConfiguration", IDS_MANAGEMENT_DEVICE_CONFIGURATION},
    {"deviceReporting", IDS_MANAGEMENT_DEVICE_REPORTING},
    {"updateRequiredEolAdminMessageTitle",
     IDS_MANAGEMENT_UPDATE_REQUIRED_EOL_ADMIN_MESSAGE_TITLE},
    {kManagementLogUploadEnabled, IDS_MANAGEMENT_LOG_UPLOAD_ENABLED},
    {kManagementReportActivityTimes,
     IDS_MANAGEMENT_REPORT_DEVICE_ACTIVITY_TIMES},
    {kManagementReportHardwareStatus,
     IDS_MANAGEMENT_REPORT_DEVICE_HARDWARE_STATUS},
    {kManagementReportNetworkInterfaces,
     IDS_MANAGEMENT_REPORT_DEVICE_NETWORK_INTERFACES},
    {kManagementReportUsers, IDS_MANAGEMENT_REPORT_DEVICE_USERS},
    {kManagementReportCrashReports, IDS_MANAGEMENT_REPORT_DEVICE_CRASH_REPORTS},
    {kManagementReportAppInfoAndActivity,
     IDS_MANAGEMENT_REPORT_APP_INFO_AND_ACTIVITY},
    {kManagementPrinting, IDS_MANAGEMENT_REPORT_PRINTING},
    {kManagementReportPrintJobs, IDS_MANAGEMENT_REPORT_PRINT_JOBS},
    {kManagementCrostini, IDS_MANAGEMENT_CROSTINI},
    {kManagementCrostiniContainerConfiguration,
     IDS_MANAGEMENT_CROSTINI_CONTAINER_CONFIGURATION},
    {kManagementReportExtensions, IDS_MANAGEMENT_REPORT_EXTENSIONS},
    {kManagementReportAndroidApplications,
     IDS_MANAGEMENT_REPORT_ANDROID_APPLICATIONS},
    {"proxyServerPrivacyDisclosure",
     IDS_MANAGEMENT_PROXY_SERVER_PRIVACY_DISCLOSURE},
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    {"browserReporting", IDS_MANAGEMENT_BROWSER_REPORTING},
    {"browserReportingExplanation",
     IDS_MANAGEMENT_BROWSER_REPORTING_EXPLANATION},
    {"extensionReporting", IDS_MANAGEMENT_EXTENSION_REPORTING},
    {"extensionReportingTitle", IDS_MANAGEMENT_EXTENSIONS_INSTALLED},
    {"extensionName", IDS_MANAGEMENT_EXTENSIONS_NAME},
    {"extensionPermissions", IDS_MANAGEMENT_EXTENSIONS_PERMISSIONS},
    {"title", IDS_MANAGEMENT_TITLE},
    {"toolbarTitle", IDS_MANAGEMENT_TOOLBAR_TITLE},
    {"searchPrompt", IDS_SETTINGS_SEARCH_PROMPT},
    {"clearSearch", IDS_CLEAR_SEARCH},
    {"backButton", IDS_ACCNAME_BACK},
    {"managedWebsites", IDS_MANAGEMENT_MANAGED_WEBSITES},
    {"managedWebsitesSubtitle", IDS_MANAGEMENT_MANAGED_WEBSITES_EXPLANATION},
    {kManagementExtensionReportMachineName,
     IDS_MANAGEMENT_EXTENSION_REPORT_MACHINE_NAME},
    {kManagementExtensionReportMachineNameAddress,
     IDS_MANAGEMENT_EXTENSION_REPORT_MACHINE_NAME_ADDRESS},
    {kManagementExtensionReportUsername,
     IDS_MANAGEMENT_EXTENSION_REPORT_USERNAME},
    {kManagementExtensionReportVersion,
     IDS_MANAGEMENT_EXTENSION_REPORT_VERSION},
    {kManagementExtensionReportExtensionsPlugin,
     IDS_MANAGEMENT_EXTENSION_REPORT_EXTENSIONS_PLUGINS},
    {kManagementExtensionReportPerfCrash,
     IDS_MANAGEMENT_EXTENSION_REPORT_PERF_CRASH},
    {kManagementExtensionReportUserBrowsingData,
     IDS_MANAGEMENT_EXTENSION_REPORT_USER_BROWSING_DATA},
    {kThreatProtectionTitle, IDS_MANAGEMENT_THREAT_PROTECTION},
    {"connectorEvent", IDS_MANAGEMENT_CONNECTORS_EVENT},
    {"connectorVisibleData", IDS_MANAGEMENT_CONNECTORS_VISIBLE_DATA},
    {kManagementEnterpriseReportingEvent,
     IDS_MANAGEMENT_ENTERPRISE_REPORTING_EVENT},
    {kManagementEnterpriseReportingVisibleData,
     IDS_MANAGEMENT_ENTERPRISE_REPORTING_VISIBLE_DATA},
    {kManagementOnFileAttachedEvent, IDS_MANAGEMENT_FILE_ATTACHED_EVENT},
    {kManagementOnFileAttachedVisibleData,
     IDS_MANAGEMENT_FILE_ATTACHED_VISIBLE_DATA},
    {kManagementOnFileDownloadedEvent, IDS_MANAGEMENT_FILE_DOWNLOADED_EVENT},
    {kManagementOnFileDownloadedVisibleData,
     IDS_MANAGEMENT_FILE_DOWNLOADED_VISIBLE_DATA},
    {kManagementOnBulkDataEntryEvent, IDS_MANAGEMENT_TEXT_ENTERED_EVENT},
    {kManagementOnBulkDataEntryVisibleData,
     IDS_MANAGEMENT_TEXT_ENTERED_VISIBLE_DATA},
    {kManagementOnPageVisitedEvent, IDS_MANAGEMENT_PAGE_VISITED_EVENT},
    {kManagementOnPageVisitedVisibleData,
     IDS_MANAGEMENT_PAGE_VISITED_VISIBLE_DATA},
  };

  source->AddLocalizedStrings(kLocalizedStrings);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  source->AddString("managementDeviceLearnMoreUrl",
                    chrome::kLearnMoreEnterpriseURL);
  source->AddString("managementAccountLearnMoreUrl",
                    chrome::kManagedUiLearnMoreUrl);
  source->AddString("pluginVmDataCollection",
                    l10n_util::GetStringFUTF16(
                        IDS_MANAGEMENT_REPORT_PLUGIN_VM,
                        l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  // Add required resources.
  source->AddResourcePath("management_browser_proxy.js",
                          IDR_MANAGEMENT_BROWSER_PROXY_JS);
  source->AddResourcePath("management_ui.js", IDR_MANAGEMENT_UI_JS);
  source->AddResourcePath("icons.js", IDR_MANAGEMENT_ICONS_JS);
  source->SetDefaultResource(IDR_MANAGEMENT_HTML);
  return source;
}

}  // namespace

// static
base::RefCountedMemory* ManagementUI::GetFaviconResourceBytes(
    ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_MANAGEMENT_FAVICON, scale_factor);
}

// static
std::u16string ManagementUI::GetManagementPageSubtitle(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
  if (!connector->IsEnterpriseManaged() &&
      !profile->GetProfilePolicyConnector()->IsManaged()) {
    return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                      l10n_util::GetStringUTF16(device_type));
  }

  std::string account_manager = connector->GetEnterpriseDomainManager();

  if (account_manager.empty())
    account_manager = connector->GetRealm();
  if (account_manager.empty())
    account_manager = ManagementUIHandler::GetAccountManager(profile);
  if (account_manager.empty()) {
    return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED,
                                      l10n_util::GetStringUTF16(device_type));
  }
  return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                    l10n_util::GetStringUTF16(device_type),
                                    base::UTF8ToUTF16(account_manager));
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  const auto account_manager = ManagementUIHandler::GetAccountManager(profile);
  const auto managed =
      profile->GetProfilePolicyConnector()->IsManaged() ||
      g_browser_process->browser_policy_connector()->HasMachineLevelPolicies();
  if (account_manager.empty()) {
    return l10n_util::GetStringUTF16(managed
                                         ? IDS_MANAGEMENT_SUBTITLE
                                         : IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE);
  }
  if (managed) {
    return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                      base::UTF8ToUTF16(account_manager));
  }
  return l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

ManagementUI::ManagementUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  content::WebUIDataSource* source =
      CreateManagementUIHtmlSource(Profile::FromWebUI(web_ui));
  ManagementUIHandler::Initialize(web_ui, source);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

ManagementUI::~ManagementUI() {}
