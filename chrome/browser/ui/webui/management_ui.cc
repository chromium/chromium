// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui.h"

#include <memory>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/browser/ui/webui/management_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/safe_browsing/common/safebrowsing_constants.h"
#include "components/strings/grit/components_strings.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/grit/chromium_strings.h"
#include "ui/chromeos/devicetype_utils.h"
#else  // defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#endif  // defined(OS_CHROMEOS)

namespace {

content::WebUIDataSource* CreateManagementUIHtmlSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIManagementHost);

  source->AddString("pageSubtitle",
                    ManagementUI::GetManagementPageSubtitle(profile));

  static constexpr LocalizedString kLocalizedStrings[] = {
#if defined(OS_CHROMEOS)
    {"learnMore", IDS_LEARN_MORE},
    {"localTrustRoots", IDS_MANAGEMENT_LOCAL_TRUST_ROOTS},
    {"managementTrustRootsConfigured", IDS_MANAGEMENT_TRUST_ROOTS_CONFIGURED},
    {"deviceConfiguration", IDS_MANAGEMENT_DEVICE_CONFIGURATION},
    {"deviceReporting", IDS_MANAGEMENT_DEVICE_REPORTING},
    {kManagementLogUploadEnabled, IDS_MANAGEMENT_LOG_UPLOAD_ENABLED},
    {kManagementReportActivityTimes,
     IDS_MANAGEMENT_REPORT_DEVICE_ACTIVITY_TIMES},
    {kManagementReportHardwareStatus,
     IDS_MANAGEMENT_REPORT_DEVICE_HARDWARE_STATUS},
    {kManagementReportNetworkInterfaces,
     IDS_MANAGEMENT_REPORT_DEVICE_NETWORK_INTERFACES},
    {kManagementReportUsers, IDS_MANAGEMENT_REPORT_DEVICE_USERS},
    {kManagementPrinting, IDS_MANAGEMENT_REPORT_PRINTING},
    {kManagementCrostini, IDS_MANAGEMENT_CROSTINI},
#endif  // defined(OS_CHROMEOS)
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
    {"clearSearch", IDS_DOWNLOAD_CLEAR_SEARCH},
    {"backButton", IDS_ACCNAME_BACK},
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
    {kManagementDataLossPreventionName,
     IDS_MANAGEMENT_DATA_LOSS_PREVENTION_NAME},
    {kManagementDataLossPreventionPermissions,
     IDS_MANAGEMENT_DATA_LOSS_PREVENTION_PERMISSIONS},
    {kManagementMalwareScanningName, IDS_MANAGEMENT_MALWARE_SCANNING_NAME},
    {kManagementMalwareScanningPermissions,
     IDS_MANAGEMENT_MALWARE_SCANNING_PERMISSIONS},
    {kManagementEnterpriseReportingName,
     IDS_MANAGEMENT_ENTERPRISE_REPORTING_NAME},
    {kManagementEnterpriseReportingPermissions,
     IDS_MANAGEMENT_ENTERPRISE_REPORTING_PERMISSIONS},
  };

  AddLocalizedStringsBulk(source, kLocalizedStrings,
                          base::size(kLocalizedStrings));

  source->AddString(kManagementExtensionReportSafeBrowsingWarnings,
                    l10n_util::GetStringFUTF16(
                        IDS_MANAGEMENT_EXTENSION_REPORT_SAFE_BROWSING_WARNINGS,
                        base::UTF8ToUTF16(safe_browsing::kSafeBrowsingUrl)));
#if defined(OS_CHROMEOS)
  source->AddString("managementDeviceLearnMoreUrl",
                    chrome::kLearnMoreEnterpriseURL);
  source->AddString("managementAccountLearnMoreUrl",
                    chrome::kManagedUiLearnMoreUrl);
#endif  // defined(OS_CHROMEOS)

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
base::string16 ManagementUI::GetManagementPageSubtitle(Profile* profile) {
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  const auto device_type = ui::GetChromeOSDeviceTypeResourceId();
  if (!connector->IsEnterpriseManaged() &&
      !profile->GetProfilePolicyConnector()->IsManaged()) {
    return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE,
                                      l10n_util::GetStringUTF16(device_type));
  }

  std::string display_domain = connector->GetEnterpriseDisplayDomain();

  if (display_domain.empty())
    display_domain = connector->GetRealm();
  if (display_domain.empty())
    display_domain = ManagementUIHandler::GetAccountDomain(profile);
  if (display_domain.empty()) {
    return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED,
                                      l10n_util::GetStringUTF16(device_type));
  }
  return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                    l10n_util::GetStringUTF16(device_type),
                                    base::UTF8ToUTF16(display_domain));
#else   // defined(OS_CHROMEOS)
  const auto management_domain = ManagementUIHandler::GetAccountDomain(profile);
  const auto managed =
      profile->GetProfilePolicyConnector()->IsManaged() ||
      g_browser_process->browser_policy_connector()->HasMachineLevelPolicies();
  if (management_domain.empty()) {
    return l10n_util::GetStringUTF16(managed
                                         ? IDS_MANAGEMENT_SUBTITLE
                                         : IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE);
  }
  if (managed) {
    return l10n_util::GetStringFUTF16(IDS_MANAGEMENT_SUBTITLE_MANAGED_BY,
                                      base::UTF8ToUTF16(management_domain));
  }
  return l10n_util::GetStringUTF16(IDS_MANAGEMENT_NOT_MANAGED_SUBTITLE);
#endif  // defined(OS_CHROMEOS)
}

ManagementUI::ManagementUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  content::WebUIDataSource* source =
      CreateManagementUIHtmlSource(Profile::FromWebUI(web_ui));
  ManagementUIHandler::Initialize(web_ui, source);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);
}

ManagementUI::~ManagementUI() {}
