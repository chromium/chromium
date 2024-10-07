// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_pages.h"

#include <stddef.h>

#include <memory>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/map_util.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/file_system_access/file_system_access_features.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/user_education/show_promo_in_page.h"
#include "chrome/browser/ui/webui/bookmarks/bookmarks_ui.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/user_education/tutorial_identifiers.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/core/common/safe_browsing_settings_metrics.h"
#include "components/safe_browsing/core/common/safebrowsing_referral_methods.h"
#include "components/signin/public/base/consent_level.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_urls.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/webui/settings/public/constants/routes_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#else
#include "chrome/browser/ui/signin/signin_view_controller.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/web_applications/web_app_utils.h"
#endif

using base::UserMetricsAction;

namespace chrome {
namespace {

const char kHashMark[] = "#";

void FocusWebContents(Browser* browser) {
  auto* const contents = browser->tab_strip_model()->GetActiveWebContents();
  if (contents)
    contents->Focus();
}

// Shows |url| in a tab in |browser|. If a tab is already open to |url|,
// ignoring the URL path, then that tab becomes selected. Overwrites the new tab
// page if it is open.
void ShowSingletonTabIgnorePathOverwriteNTP(Browser* browser, const GURL& url) {
  ShowSingletonTabOverwritingNTP(browser, url,
                                 NavigateParams::IGNORE_AND_NAVIGATE);
}

void OpenBookmarkManagerForNode(Browser* browser, int64_t node_id) {
  GURL url = GURL(kChromeUIBookmarksURL)
                 .Resolve(base::StringPrintf(
                     "/?id=%s", base::NumberToString(node_id).c_str()));
  ShowSingletonTabIgnorePathOverwriteNTP(browser, url);
}

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
void LaunchReleaseNotesImpl(Profile* profile, apps::LaunchSource source) {
  base::RecordAction(UserMetricsAction("ReleaseNotes.ShowReleaseNotes"));
  ash::SystemAppLaunchParams params;
  params.url =
      base::FeatureList::IsEnabled(
          ash::features::kHelpAppOpensInsteadOfReleaseNotesNotification) &&
              source == apps::LaunchSource::kFromReleaseNotesNotification
          ? GURL("chrome://help-app/updates?launchSource=version-update")
          : GURL("chrome://help-app/updates");
  params.launch_source = source;
  LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::HELP, params);
}
#endif

// Shows either the help app or the appropriate help page for |source|. If
// |browser| is NULL and the help page is used (vs the app), the help page is
// shown in the last active browser. If there is no such browser, a new browser
// is created.
void ShowHelpImpl(Browser* browser, Profile* profile, HelpSource source) {
  base::RecordAction(UserMetricsAction("ShowHelpTab"));
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto app_launch_source = apps::LaunchSource::kUnknown;
  switch (source) {
    case HELP_SOURCE_KEYBOARD:
      app_launch_source = apps::LaunchSource::kFromKeyboard;
      break;
    case HELP_SOURCE_MENU:
      app_launch_source = apps::LaunchSource::kFromMenu;
      break;
    case HELP_SOURCE_WEBUI:
    case HELP_SOURCE_WEBUI_CHROME_OS:
      app_launch_source = apps::LaunchSource::kFromOtherApp;
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unhandled help source" << source;
  }

  ash::SystemAppLaunchParams params;
  params.launch_source = app_launch_source;
  LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::HELP, params);
#else
  GURL url;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // If this is Lacros, forward the request to Ash.
  url = GURL(kOsUIHelpAppURL);
#else
  switch (source) {
    case HELP_SOURCE_KEYBOARD:
      url = GURL(kChromeHelpViaKeyboardURL);
      break;
    case HELP_SOURCE_MENU:
      url = GURL(kChromeHelpViaMenuURL);
      break;
    case HELP_SOURCE_WEBHID:
      url = GURL(kChooserHidOverviewUrl);
      break;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case HELP_SOURCE_WEBUI:
      url = GURL(kChromeHelpViaWebUIURL);
      break;
    case HELP_SOURCE_WEBUI_CHROME_OS:
      url = GURL(kChromeOsHelpViaWebUIURL);
      break;
#else
    case HELP_SOURCE_WEBUI:
      url = GURL(kChromeHelpViaWebUIURL);
      break;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    case HELP_SOURCE_WEBUSB:
      url = GURL(kChooserUsbOverviewURL);
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "Unhandled help source " << source;
  }
#endif  // BUILDFLAG_IS_CHROMEOS_LACROS)
  if (browser) {
    ShowSingletonTab(browser, url);
  } else {
    ShowSingletonTab(profile, url);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::string GenerateContentSettingsExceptionsSubPage(ContentSettingsType type) {
  // In MD Settings, the exceptions no longer have a separate subpage.
  // This list overrides the group names defined in site_settings_helper for the
  // purposes of URL generation for MD Settings only. We need this because some
  // of the old group names are no longer appropriate.
  //
  // TODO(crbug.com/40523530): Update the group names defined in
  // site_settings_helper once Options is removed from Chrome. Then this list
  // will no longer be needed.

  static constexpr auto kSettingsPathOverrides =
      base::MakeFixedFlatMap<ContentSettingsType, std::string_view>({
          {ContentSettingsType::AUTOMATIC_DOWNLOADS, "automaticDownloads"},
          {ContentSettingsType::BACKGROUND_SYNC, "backgroundSync"},
          {ContentSettingsType::MEDIASTREAM_MIC, "microphone"},
          {ContentSettingsType::MEDIASTREAM_CAMERA, "camera"},
          {ContentSettingsType::MIDI_SYSEX, "midiDevices"},
          {ContentSettingsType::ADS, "ads"},
          {ContentSettingsType::HID_CHOOSER_DATA, "hidDevices"},
          {ContentSettingsType::STORAGE_ACCESS, "storageAccess"},
          {ContentSettingsType::USB_CHOOSER_DATA, "usbDevices"},
          {ContentSettingsType::WEB_PRINTING, "webPrinting"},
      });

  const std::string_view* override =
      base::FindOrNull(kSettingsPathOverrides, type);
  return base::StrCat(
      {kContentSettingsSubPage, "/",
       override ? *override
                : site_settings::ContentSettingsTypeToGroupName(type)});
}

bool SiteGURLIsValid(const GURL& url) {
  url::Origin site_origin = url::Origin::Create(url);
  // TODO(crbug.com/40399136): Site Details should work with file:// urls
  // when this bug is fixed, so add it to the allowlist when that happens.
  return !site_origin.opaque() && (url.SchemeIsHTTPOrHTTPS() ||
                                   url.SchemeIs(extensions::kExtensionScheme) ||
                                   url.SchemeIs(chrome::kIsolatedAppScheme));
}

void ShowSiteSettingsImpl(Browser* browser, Profile* profile, const GURL& url) {
  // If a valid non-file origin, open a settings page specific to the current
  // origin of the page. Otherwise, open Content Settings.
  constexpr char kParamRequest[] = "site";
  GURL link_destination = GetSettingsUrl(chrome::kContentSettingsSubPage);
  if (SiteGURLIsValid(url)) {
    std::string origin_string = url::Origin::Create(url).Serialize();
    link_destination =
        net::AppendQueryParameter(GetSettingsUrl(chrome::kSiteDetailsSubpage),
                                  kParamRequest, origin_string);
  }
  NavigateParams params(profile, link_destination, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.browser = browser;
  Navigate(&params);
}

// TODO(crbug.com/40101962): Add a browsertest that parallels the existing site
// settings browsertests that open the page info button, and click through to
// the file system site settings page for a given origin.
void ShowSiteSettingsFileSystemImpl(Browser* browser,
                                    Profile* profile,
                                    const GURL& url) {
  constexpr char kParamRequest[] = "site";
  GURL link_destination = GetSettingsUrl(chrome::kFileSystemSettingsSubpage);

  // If origin is valid, open a file system site settings page specific to the
  // current origin of the page. Otherwise, open the File System Site Settings
  // page.
  if (base::FeatureList::IsEnabled(
          features::kFileSystemAccessPersistentPermissions) &&
      SiteGURLIsValid(url)) {
    // TODO(crbug.com/40946480): Update `origin_string` to remove the encoded
    // trailing slash, once it's no longer required to correctly navigate to
    // file system site settings page for the given origin.
    const std::string origin_string =
        base::StrCat({url::Origin::Create(url).Serialize(), "/"});
    link_destination = net::AppendQueryParameter(link_destination,
                                                 kParamRequest, origin_string);
  }
  NavigateParams params(profile, link_destination, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.browser = browser;
  Navigate(&params);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ShowSystemAppInternal(Profile* profile,
                           const ash::SystemWebAppType type,
                           const ash::SystemAppLaunchParams& params) {
  ash::LaunchSystemWebAppAsync(profile, type, params);
}
void ShowSystemAppInternal(Profile* profile, const ash::SystemWebAppType type) {
  ash::SystemAppLaunchParams params;
  params.launch_source = apps::LaunchSource::kUnknown;
  ash::LaunchSystemWebAppAsync(profile, type, params);
}
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
void ShowSystemAppInternal(Profile* profile, const GURL& url) {
  ShowSingletonTab(profile, url);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

void ShowBookmarkManager(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  ShowSingletonTabIgnorePathOverwriteNTP(browser, GURL(kChromeUIBookmarksURL));
}

void ShowBookmarkManagerForNode(Browser* browser, int64_t node_id) {
  base::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  OpenBookmarkManagerForNode(browser, node_id);
}

void ShowHistory(Browser* browser, const std::string& host_name) {
  // History UI should not be shown in Incognito mode, instead history
  // disclaimer bubble should show up. This also updates the behavior of history
  // keyboard shortcts in Incognito.
  if (browser->profile()->IsOffTheRecord()) {
    browser->window()->ShowIncognitoHistoryDisclaimerDialog();
    return;
  }

  base::RecordAction(UserMetricsAction("ShowHistory"));
  GURL url = GURL(kChromeUIHistoryURL);
  if (!host_name.empty()) {
    GURL::Replacements replacements;
    std::string query("q=");
    query += base::EscapeQueryParamValue(base::StrCat({"host:", host_name}),
                                         /*use_plus=*/false);
    replacements.SetQueryStr(query);
    url = url.ReplaceComponents(replacements);
  }
  ShowSingletonTabIgnorePathOverwriteNTP(browser, url);
}

void ShowHistory(Browser* browser) {
  ShowHistory(browser, std::string());
}

void ShowDownloads(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowDownloads"));
  if (browser->window() && browser->window()->IsDownloadShelfVisible())
    browser->window()->GetDownloadShelf()->Close();
  ShowSingletonTabOverwritingNTP(browser, GURL(kChromeUIDownloadsURL));
}

void ShowExtensions(Browser* browser,
                    const std::string& extension_to_highlight) {
  base::RecordAction(UserMetricsAction("ShowExtensions"));
  GURL url(kChromeUIExtensionsURL);
  if (!extension_to_highlight.empty()) {
    GURL::Replacements replacements;
    std::string query("id=");
    query += extension_to_highlight;
    replacements.SetQueryStr(query);
    url = url.ReplaceComponents(replacements);
  }
  ShowSingletonTabIgnorePathOverwriteNTP(browser, url);
}

void ShowHelp(Browser* browser, HelpSource source) {
  ShowHelpImpl(browser, browser->profile(), source);
}

void ShowHelpForProfile(Profile* profile, HelpSource source) {
  ShowHelpImpl(nullptr, profile, source);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void ShowChromeTips(Browser* browser) {
  static const char kChromeTipsURL[] = "https://www.google.com/chrome/tips/";
  ShowSingletonTab(browser, GURL(kChromeTipsURL));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void ShowChromeWhatsNew(Browser* browser) {
  ShowSingletonTab(browser, GURL(kChromeUIWhatsNewURL));
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

void LaunchReleaseNotes(Profile* profile, apps::LaunchSource source) {
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  LaunchReleaseNotesImpl(profile, source);
#endif
}

void ShowBetaForum(Browser* browser) {
  ShowSingletonTab(browser, GURL(kChromeBetaForumURL));
}

void ShowSlow(Browser* browser) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ShowSingletonTab(browser, GURL(kChromeUISlowURL));
#endif
}

GURL GetSettingsUrl(const std::string& sub_page) {
  return GURL(std::string(kChromeUISettingsURL) + sub_page);
}

bool IsTrustedPopupWindowWithScheme(const Browser* browser,
                                    const std::string& scheme) {
  if (browser->is_type_normal() || !browser->is_trusted_source())
    return false;
  if (scheme.empty())  // Any trusted popup window
    return true;
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetWebContentsAt(0);
  if (!web_contents)
    return false;
  GURL url(web_contents->GetURL());
  return url.SchemeIs(scheme);
}

void ShowSettings(Browser* browser) {
  ShowSettingsSubPage(browser, std::string());
}

void ShowSettingsSubPage(Browser* browser, const std::string& sub_page) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ShowSettingsSubPageForProfile(browser->profile(), sub_page);
#else
  ShowSettingsSubPageInTabbedBrowser(browser, sub_page);
#endif
}

void ShowSettingsSubPageForProfile(Profile* profile,
                                   const std::string& sub_page) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // OS settings sub-pages are handled else where and should never be
  // encountered here.
  DCHECK(!chromeos::settings::IsOSSettingsSubPage(sub_page)) << sub_page;
#endif
  Browser* browser = chrome::FindTabbedBrowser(profile, false);
  if (!browser)
    browser = Browser::Create(Browser::CreateParams(profile, true));
  ShowSettingsSubPageInTabbedBrowser(browser, sub_page);
}

void ShowSettingsSubPageInTabbedBrowser(Browser* browser,
                                        const std::string& sub_page) {
  base::RecordAction(UserMetricsAction("ShowOptions"));

  // Since the user may be triggering navigation from another UI element such as
  // a menu, ensure the web contents (and therefore the settings page that is
  // about to be shown) is focused. (See crbug/926492 for motivation.)
  FocusWebContents(browser);
  ShowSingletonTabIgnorePathOverwriteNTP(browser, GetSettingsUrl(sub_page));
}

void ShowContentSettingsExceptions(Browser* browser,
                                   ContentSettingsType content_settings_type) {
  ShowSettingsSubPage(
      browser, GenerateContentSettingsExceptionsSubPage(content_settings_type));
}

void ShowContentSettingsExceptionsForProfile(
    Profile* profile,
    ContentSettingsType content_settings_type) {
  ShowSettingsSubPageForProfile(
      profile, GenerateContentSettingsExceptionsSubPage(content_settings_type));
}

void ShowSiteSettings(Browser* browser, const GURL& url) {
  ShowSiteSettingsImpl(browser, browser->profile(), url);
}

void ShowSiteSettings(Profile* profile, const GURL& url) {
  DCHECK(profile);
  ShowSiteSettingsImpl(nullptr, profile, url);
}

void ShowSiteSettingsFileSystem(Browser* browser, const GURL& url) {
  ShowSiteSettingsFileSystemImpl(browser, browser->profile(), url);
}

void ShowSiteSettingsFileSystem(Profile* profile, const GURL& url) {
  DCHECK(profile);
  ShowSiteSettingsFileSystemImpl(nullptr, profile, url);
}

void ShowContentSettings(Browser* browser,
                         ContentSettingsType content_settings_type) {
  ShowSettingsSubPage(
      browser, base::StrCat({kContentSettingsSubPage, kHashMark,
                             site_settings::ContentSettingsTypeToGroupName(
                                 content_settings_type)}));
}

void ShowClearBrowsingDataDialog(Browser* browser) {
  base::RecordAction(UserMetricsAction("ClearBrowsingData_ShowDlg"));
  ShowSettingsSubPage(browser, kClearBrowserDataSubPage);
}

void ShowPasswordManager(Browser* browser) {
  base::RecordAction(UserMetricsAction("Options_ShowPasswordManager"));
  // This code is necessary to fix a bug (crbug.com/1448559) during Password
  // Manager Shortcut tutorial flow.
  auto* service =
      UserEducationServiceFactory::GetForBrowserContext(browser->profile());
  if (service) {
    auto* tutorial_service = &service->tutorial_service();
    if (tutorial_service &&
        tutorial_service->IsRunningTutorial(kPasswordManagerTutorialId)) {
      ShowSingletonTab(browser, GURL(kChromeUIPasswordManagerSettingsURL));
      return;
    }
  }
  ShowSingletonTabIgnorePathOverwriteNTP(browser,
                                         GURL(kChromeUIPasswordManagerURL));
}

void ShowPasswordDetailsPage(Browser* browser,
                             const std::string& password_domain_name) {
  base::RecordAction(
      UserMetricsAction("Options_ShowPasswordDetailsInPasswordManager"));
  std::string url = base::StrCat(
      {GetGooglePasswordManagerSubPageURLStr(), "/", password_domain_name});
  ShowSingletonTabIgnorePathOverwriteNTP(browser, GURL(url));
}

void ShowPasswordCheck(Browser* browser) {
  base::RecordAction(UserMetricsAction("Options_ShowPasswordCheck"));
  ShowSingletonTabIgnorePathOverwriteNTP(
      browser, GURL(kChromeUIPasswordManagerCheckupURL));
}

void ShowSafeBrowsingEnhancedProtection(Browser* browser) {
  safe_browsing::LogShowEnhancedProtectionAction();
  ShowSettingsSubPage(browser, kSafeBrowsingEnhancedProtectionSubPage);
}

void ShowSafeBrowsingEnhancedProtectionWithIph(
    Browser* browser,
    safe_browsing::SafeBrowsingSettingReferralMethod referral_method) {
#if BUILDFLAG(FULL_SAFE_BROWSING)
  ShowPromoInPage::Params params;
  params.target_url =
      chrome::GetSettingsUrl(chrome::kSafeBrowsingEnhancedProtectionSubPage);
  params.bubble_anchor_id = kEnhancedProtectionSettingElementId;
  params.bubble_arrow = user_education::HelpBubbleArrow::kBottomLeft;
  params.bubble_text = l10n_util::GetStringUTF16(
      IDS_SETTINGS_SAFEBROWSING_ENHANCED_IPH_BUBBLE_TEXT);
  params.close_button_alt_text_id =
      IDS_SETTINGS_SAFEBROWSING_ENHANCED_IPH_BUBBLE_CLOSE_BUTTON_ARIA_LABEL_TEXT;
  base::UmaHistogramEnumeration("SafeBrowsing.EsbPromotionFlow.IphShown",
                                referral_method);
  safe_browsing::LogShowEnhancedProtectionAction();
  ShowPromoInPage::Start(browser, std::move(params));
#endif
}

void ShowImportDialog(Browser* browser) {
  base::RecordAction(UserMetricsAction("Import_ShowDlg"));
  ShowSettingsSubPage(browser, kImportDataSubPage);
}

void ShowAboutChrome(Browser* browser) {
  base::RecordAction(UserMetricsAction("AboutChrome"));
  ShowSingletonTabIgnorePathOverwriteNTP(browser, GURL(kChromeUIHelpURL));
}

void ShowSearchEngineSettings(Browser* browser) {
  base::RecordAction(UserMetricsAction("EditSearchEngines"));
  ShowSettingsSubPage(browser, kSearchEnginesSubPage);
}

void ShowWebStore(Browser* browser, std::string_view utm_source_value) {
  GURL webstore_url = extension_urls::GetWebstoreLaunchURL();
  // TODO(crbug.com/40073814): Refactor this check into
  // extension_urls::GetWebstoreLaunchURL() and fix tests relying on it.
  if (base::FeatureList::IsEnabled(extensions_features::kNewWebstoreURL)) {
    webstore_url = extension_urls::GetNewWebstoreLaunchURL();
  }
  ShowSingletonTabIgnorePathOverwriteNTP(
      browser, extension_urls::AppendUtmSource(webstore_url, utm_source_value));
}

void ShowPrivacySandboxSettings(Browser* browser) {
  base::RecordAction(UserMetricsAction("Options_ShowPrivacySandbox"));
  ShowSettingsSubPage(browser, kAdPrivacySubPage);
}

void ShowPrivacySandboxAdMeasurementSettings(Browser* browser) {
  base::RecordAction(UserMetricsAction("Options_ShowPrivacySandbox"));
  ShowSettingsSubPage(browser, kPrivacySandboxMeasurementSubpage);
}

void ShowAddresses(Browser* browser) {
  base::RecordAction(UserMetricsAction("Options_ShowAddresses"));
  ShowSettingsSubPage(browser, kAddressesSubPage);
}

void ShowPaymentMethods(Browser* browser) {
  base::RecordAction(UserMetricsAction("Options_ShowPaymentMethods"));
  ShowSettingsSubPage(browser, kPaymentsSubPage);
}

void ShowAllSitesSettingsFilteredByRwsOwner(
    Browser* browser,
    const std::string& rws_owner_host_name) {
  GURL url = GetSettingsUrl(kAllSitesSettingsSubpage);
  if (!rws_owner_host_name.empty()) {
    GURL::Replacements replacements;
    std::string query("searchSubpage=");
    query += base::EscapeQueryParamValue(
        base::StrCat({"related:", rws_owner_host_name}),
        /*use_plus=*/false);
    replacements.SetQueryStr(query);
    url = url.ReplaceComponents(replacements);
  }

  ShowSingletonTabIgnorePathOverwriteNTP(browser, url);
}

void ShowEnterpriseManagementPageInTabbedBrowser(Browser* browser) {
  // Management shows in a tab because it has a "back" arrow that takes the
  // user to the Chrome browser about page, which is part of browser settings.
  ShowSingletonTabIgnorePathOverwriteNTP(browser, GURL(kChromeUIManagementURL));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ShowAppManagementPage(Profile* profile,
                           const std::string& app_id,
                           ash::settings::AppManagementEntryPoint entry_point) {
  // This histogram is also declared and used at chrome/browser/resources/
  // settings/chrome_os/os_apps_page/app_management_page/constants.js.
  constexpr char kAppManagementEntryPointsHistogramName[] =
      "AppManagement.EntryPoints";

  base::UmaHistogramEnumeration(kAppManagementEntryPointsHistogramName,
                                entry_point);
  std::string sub_page = base::StrCat(
      {chromeos::settings::mojom::kAppDetailsSubpagePath, "?id=", app_id});
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile,
                                                               sub_page);
}

void ShowGraduationApp(Profile* profile) {
  ShowSystemAppInternal(profile, ash::SystemWebAppType::GRADUATION);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
GURL GetOSSettingsUrl(const std::string& sub_page) {
  DCHECK(sub_page.empty() || chromeos::settings::IsOSSettingsSubPage(sub_page))
      << sub_page;
  return GURL(std::string(kChromeUIOSSettingsURL) + sub_page);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void ShowPrintManagementApp(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ShowSystemAppInternal(profile, ash::SystemWebAppType::PRINT_MANAGEMENT);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  ShowSystemAppInternal(profile, GURL(kOsUIPrintManagementAppURL));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShowConnectivityDiagnosticsApp(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ShowSystemAppInternal(profile,
                        ash::SystemWebAppType::CONNECTIVITY_DIAGNOSTICS);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  ShowSystemAppInternal(profile, GURL(kOsUIConnectivityDiagnosticsAppURL));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShowScanningApp(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ShowSystemAppInternal(profile, ash::SystemWebAppType::SCANNING);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  ShowSystemAppInternal(profile, GURL(kOsUIScanningAppURL));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShowDiagnosticsApp(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ShowSystemAppInternal(profile, ash::SystemWebAppType::DIAGNOSTICS);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  ShowSystemAppInternal(profile, GURL(kOsUIDiagnosticsAppURL));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShowFirmwareUpdatesApp(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ShowSystemAppInternal(profile, ash::SystemWebAppType::FIRMWARE_UPDATE);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  ShowSystemAppInternal(profile, GURL(kOsUIFirmwareUpdaterAppURL));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShowShortcutCustomizationApp(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ShowSystemAppInternal(profile, ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  ShowSystemAppInternal(profile, GURL(kOsUIShortcutCustomizationAppURL));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShowShortcutCustomizationApp(Profile* profile,
                                  const std::string& action,
                                  const std::string& category) {
  const std::string query_string =
      base::StrCat({"action=", action, "&category=", category});
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::SystemAppLaunchParams params;
  params.launch_source = apps::LaunchSource::kUnknown;
  params.url = GURL(base::StrCat(
      {ash::kChromeUIShortcutCustomizationAppURL, "?", query_string}));
  ShowSystemAppInternal(profile, ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION,
                        params);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  const GURL os_shortcuts_app_url{
      base::StrCat({kOsUIShortcutCustomizationAppURL, "?", query_string})};
  ShowSystemAppInternal(profile, os_shortcuts_app_url);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
void ShowWebAppSettingsImpl(Browser* browser,
                            Profile* profile,
                            const std::string& app_id,
                            web_app::AppSettingsPageEntryPoint entry_point) {
  base::UmaHistogramEnumeration(
      web_app::kAppSettingsPageEntryPointsHistogramName, entry_point);

  const GURL link_destination(chrome::kChromeUIWebAppSettingsURL + app_id);
  NavigateParams params(profile, link_destination, ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.browser = browser;
  Navigate(&params);
}

void ShowWebAppSettings(Browser* browser,
                        const std::string& app_id,
                        web_app::AppSettingsPageEntryPoint entry_point) {
  ShowWebAppSettingsImpl(browser, browser->profile(), app_id, entry_point);
}

void ShowWebAppSettings(Profile* profile,
                        const std::string& app_id,
                        web_app::AppSettingsPageEntryPoint entry_point) {
  ShowWebAppSettingsImpl(/*browser=*/nullptr, profile, app_id, entry_point);
}
#endif

}  // namespace chrome
