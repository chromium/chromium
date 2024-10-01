// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/management/management_ui.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/management/management_ui_constants.h"
#include "chrome/browser/ui/webui/management/management_ui_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/management_resources.h"
#include "chrome/grit/management_resources_map.h"
#include "chrome/grit/theme_resources.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/grit/branded_strings.h"
#include "ui/chromeos/devicetype_utils.h"
#else  // BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

content::WebUIDataSource* CreateAndAddManagementUIHtmlSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIManagementHost);

  source->AddString("pageSubtitle",
                    ManagementUI::GetManagementPageSubtitle(profile));

  std::vector<webui::LocalizedString> localized_strings;
  ManagementUI::GetLocalizedStrings(localized_strings);
  source->AddLocalizedStrings(localized_strings);

  source->SetDefaultResource(IDR_MANAGEMENT_MANAGEMENT_HTML);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  source->AddString("managementDeviceLearnMoreUrl",
                    chrome::kLearnMoreEnterpriseURL);
  source->AddString("managementAccountLearnMoreUrl",
                    chrome::kManagedUiLearnMoreUrl);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  const size_t dlp_events_count =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile() &&
              policy::DlpRulesManagerFactory::GetForPrimaryProfile()
                  ->GetReportingManager()
          ? policy::DlpRulesManagerFactory::GetForPrimaryProfile()
                ->GetReportingManager()
                ->events_reported()
          : 0;
  source->AddString(kManagementReportDlpEvents,
                    l10n_util::GetPluralStringFUTF16(
                        IDS_MANAGEMENT_REPORT_DLP_EVENTS, dlp_events_count));
  source->AddString("pluginVmDataCollection",
                    l10n_util::GetStringFUTF16(
                        IDS_MANAGEMENT_REPORT_PLUGIN_VM,
                        l10n_util::GetStringUTF16(IDS_PLUGIN_VM_APP_NAME)));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

  webui::SetupWebUIDataSource(
      source, base::make_span(kManagementResources, kManagementResourcesSize),
      IDR_MANAGEMENT_MANAGEMENT_HTML);
  return source;
}

}  // namespace

// static
base::RefCountedMemory* ManagementUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_MANAGEMENT_FAVICON, scale_factor);
}

// static
std::u16string ManagementUI::GetManagementPageSubtitle(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
  if (!connector->IsDeviceEnterpriseManaged() &&
      !profile->GetProfilePolicyConnector()->IsManaged()) {
    return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                      l10n_util::GetStringUTF16(device_type));
  }

  std::string account_manager = connector->GetEnterpriseDomainManager();

  if (account_manager.empty()) {
    account_manager =
        chrome::GetAccountManagerIdentity(profile).value_or(std::string());
  }
  if (account_manager.empty()) {
    return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED,
                                      l10n_util::GetStringUTF16(device_type));
  }
  return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                    l10n_util::GetStringUTF16(device_type),
                                    base::UTF8ToUTF16(account_manager));
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  return chrome::GetManagementPageSubtitle(profile);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// static
void ManagementUI::GetLocalizedStrings(
    std::vector<webui::LocalizedString>& strings) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
#if BUILDFLAG(IS_CHROMEOS)
      {"learnMore", IDS_LEARN_MORE},
      {"localTrustRoots", IDS_MANAGEMENT_LOCAL_TRUST_ROOTS},
      {"filesCloudUpload", IDS_MANAGEMENT_FILES_CLOUD_UPLOAD},
      {"managementTrustRootsConfigured", IDS_MANAGEMENT_TRUST_ROOTS_CONFIGURED},
      {"deviceConfiguration", IDS_MANAGEMENT_DEVICE_CONFIGURATION},
      {"deviceReporting", IDS_MANAGEMENT_DEVICE_REPORTING},
      {"updateRequiredEolAdminMessageTitle",
       IDS_MANAGEMENT_UPDATE_REQUIRED_EOL_ADMIN_MESSAGE_TITLE},
      {kManagementLogUploadEnabled, IDS_MANAGEMENT_LOG_UPLOAD_ENABLED},
      {kManagementLogUploadEnabledNoLink,
       IDS_MANAGEMENT_LOG_UPLOAD_ENABLED_NO_LINK},
      {kManagementReportActivityTimes,
       IDS_MANAGEMENT_REPORT_DEVICE_ACTIVITY_TIMES},
      {kManagementReportNetworkData, IDS_MANAGEMENT_REPORT_DEVICE_NETWORK_DATA},
      {kManagementReportHardwareData,
       IDS_MANAGEMENT_REPORT_DEVICE_HARDWARE_DATA},
      {kManagementReportUsers, IDS_MANAGEMENT_REPORT_DEVICE_USERS},
      {kManagementReportCrashReports,
       IDS_MANAGEMENT_REPORT_DEVICE_CRASH_REPORTS},
      {kManagementReportAppInfoAndActivity,
       IDS_MANAGEMENT_REPORT_APP_INFO_AND_ACTIVITY},
      {kManagementPrinting, IDS_MANAGEMENT_REPORT_PRINTING},
      {kManagementReportDeviceAudioStatus,
       IDS_MANAGEMENT_REPORT_DEVICE_AUDIO_STATUS},
      {kManagementReportDeviceGraphicsStatus,
       IDS_MANAGEMENT_REPORT_DEVICE_GRAPHICS_STATUS},
      {kManagementReportDevicePeripherals,
       IDS_MANAGEMENT_REPORT_DEVICE_PERIPHERALS},
      {kManagementReportPrintJobs, IDS_MANAGEMENT_REPORT_PRINT_JOBS},
      {kManagementReportLoginLogout, IDS_MANAGEMENT_REPORT_LOGIN_LOGOUT},
      {kManagementReportCRDSessions, IDS_MANAGEMENT_REPORT_CRD_SESSIONS},
      {kManagementReportAllWebsiteInfoAndActivity,
       IDS_MANAGEMENT_REPORT_ALL_WEBSITE_INFO_AND_ACTIVITY},
      {kManagementReportWebsiteInfoAndActivity,
       IDS_MANAGEMENT_REPORT_WEBSITE_INFO_AND_ACTIVITY},
      {kManagementCrostini, IDS_MANAGEMENT_CROSTINI},
      {kManagementCrostiniContainerConfiguration,
       IDS_MANAGEMENT_CROSTINI_CONTAINER_CONFIGURATION},
      {kManagementReportExtensions, IDS_MANAGEMENT_REPORT_EXTENSIONS},
      {kManagementReportAndroidApplications,
       IDS_MANAGEMENT_REPORT_ANDROID_APPLICATIONS},
      {"proxyServerPrivacyDisclosure",
       IDS_MANAGEMENT_PROXY_SERVER_PRIVACY_DISCLOSURE},
      {kManagementOnFileTransferEvent, IDS_MANAGEMENT_FILE_TRANSFER_EVENT},
      {kManagementOnFileTransferVisibleData,
       IDS_MANAGEMENT_FILE_TRANSFER_VISIBLE_DATA},
      {kManagementReportFileEvents, IDS_MANAGEMENT_REPORT_FILE_EVENTS},
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
      {kManagementScreenCaptureEvent, IDS_MANAGEMENT_SCREEN_CAPTURE_EVENT},
      {kManagementScreenCaptureData, IDS_MANAGEMENT_SCREEN_CAPTURE_DATA},
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      {kManagementDeviceSignalsDisclosure,
       IDS_MANAGEMENT_DEVICE_SIGNALS_DISCLOSURE},
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      {"browserReporting", IDS_MANAGEMENT_BROWSER_REPORTING},
      {"browserReportingExplanation",
       IDS_MANAGEMENT_BROWSER_REPORTING_EXPLANATION},
      {"extensionReporting", IDS_MANAGEMENT_EXTENSION_REPORTING},
      {"extensionReportingTitle", IDS_MANAGEMENT_EXTENSIONS_INSTALLED},
      {"extensionName", IDS_MANAGEMENT_EXTENSIONS_NAME},
      {"extensionPermissions", IDS_MANAGEMENT_EXTENSIONS_PERMISSIONS},
      {"applicationReporting", IDS_MANAGEMENT_APPLICATION_REPORTING},
      {"applicationReportingTitle", IDS_MANAGEMENT_APPLICATIONS_INSTALLED},
      {"applicationName", IDS_MANAGEMENT_APPLICATIONS_NAME},
      {"applicationPermissions", IDS_MANAGEMENT_APPLICATIONS_PERMISSIONS},
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
      {kManagementExtensionReportVisitedUrl,
       IDS_MANAGEMENT_EXTENSION_REPORT_VISITED_URL},
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
      {kManagementOnPrintEvent, IDS_MANAGEMENT_PAGE_PRINTED_EVENT},
      {kManagementOnPrintVisibleData, IDS_MANAGEMENT_PAGE_PRINTED_VISIBLE_DATA},
      {kManagementOnPageVisitedEvent, IDS_MANAGEMENT_PAGE_VISITED_EVENT},
      {kManagementOnPageVisitedVisibleData,
       IDS_MANAGEMENT_PAGE_VISITED_VISIBLE_DATA},
      {kManagementLegacyTechReport, IDS_MANAGEMENT_LEGACY_TECH_REPORT},
      {kManagementLegacyTechReportNoLink,
       IDS_MANAGEMENT_LEGACY_TECH_REPORT_NO_LINK},
      {kManagementOnExtensionTelemetryEvent,
       IDS_MANAGEMENT_EXTENSION_TELEMETRY_EVENT},
      {kManagementOnExtensionTelemetryVisibleData,
       IDS_MANAGEMENT_EXTENSION_TELEMETRY_VISIBLE_DATA},
      // Profile reporting messages
      {kProfileReportingExplanation,
       IDS_MANAGEMENT_PROFILE_REPORTING_EXPLANATION},
      {kProfileReportingOverview, IDS_MANAGEMENT_PROFILE_REPORTING_OVERVIEW},
      {kProfileReportingUsername, IDS_MANAGEMENT_PROFILE_REPORTING_USERNAME},
      {kProfileReportingBrowser, IDS_MANAGEMENT_PROFILE_REPORTING_BROWSER},
      {kProfileReportingExtension, IDS_MANAGEMENT_PROFILE_REPORTING_EXTENSION},
      {kProfileReportingPolicy, IDS_MANAGEMENT_PROFILE_REPORTING_POLICY},
      {kProfileReportingLearnMore, IDS_MANAGEMENT_PROFILE_REPORTING_LEARN_MORE},
  };

  for (auto i : kLocalizedStrings) {
    strings.push_back(i);
  }
}

ManagementUI::ManagementUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  CreateAndAddManagementUIHtmlSource(Profile::FromWebUI(web_ui));

  web_ui->AddMessageHandler(ManagementUIHandler::Create(profile));
}

ManagementUI::~ManagementUI() {}
