// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_pages.h"

#include <stddef.h>

#include <memory>

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/bookmarks/bookmarks_ui.h"
#include "chrome/browser/ui/webui/settings/site_settings_helper.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_app_id_constants.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/url_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/settings/chromeos/app_management/app_management_uma.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes_util.h"
#include "chromeos/components/connectivity_diagnostics/url_constants.h"
#else
#include "chrome/browser/ui/signin_view_controller.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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
  NavigateParams params(GetSingletonTabNavigateParams(browser, url));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, &params);
}

void OpenBookmarkManagerForNode(Browser* browser, int64_t node_id) {
  GURL url = GURL(kChromeUIBookmarksURL)
                 .Resolve(base::StringPrintf(
                     "/?id=%s", base::NumberToString(node_id).c_str()));
  ShowSingletonTabIgnorePathOverwriteNTP(browser, url);
}

#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

void LaunchReleaseNotesImpl(Profile* profile,
                            apps::mojom::LaunchSource source) {
  base::RecordAction(UserMetricsAction("ReleaseNotes.ShowReleaseNotes"));
  apps::AppServiceProxyFactory::GetForProfileRedirectInIncognito(profile)
      ->LaunchAppWithUrl(web_app::kHelpAppId, ui::EventFlags::EF_NONE,
                         GURL("chrome://help-app/updates"), source,
                         apps::MakeWindowInfo(display::kDefaultDisplayId));
}

#endif

// Shows either the help app or the appropriate help page for |source|. If
// |browser| is NULL and the help page is used (vs the app), the help page is
// shown in the last active browser. If there is no such browser, a new browser
// is created.
void ShowHelpImpl(Browser* browser, Profile* profile, HelpSource source) {
  base::RecordAction(UserMetricsAction("ShowHelpTab"));
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  auto app_launch_source = apps::mojom::LaunchSource::kUnknown;
  switch (source) {
    case HELP_SOURCE_KEYBOARD:
      app_launch_source = apps::mojom::LaunchSource::kFromKeyboard;
      break;
    case HELP_SOURCE_MENU:
      app_launch_source = apps::mojom::LaunchSource::kFromMenu;
      break;
    case HELP_SOURCE_WEBUI:
    case HELP_SOURCE_WEBUI_CHROME_OS:
      app_launch_source = apps::mojom::LaunchSource::kFromOtherApp;
      break;
    default:
      NOTREACHED() << "Unhandled help source" << source;
  }
  apps::AppServiceProxyFactory::GetForProfileRedirectInIncognito(profile)
      ->Launch(web_app::kHelpAppId, ui::EventFlags::EF_NONE, app_launch_source,
               apps::MakeWindowInfo(display::kDefaultDisplayId));
#else
  GURL url;
  switch (source) {
    case HELP_SOURCE_KEYBOARD:
      url = GURL(kChromeHelpViaKeyboardURL);
      break;
    case HELP_SOURCE_MENU:
      url = GURL(kChromeHelpViaMenuURL);
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
#endif
    default:
      NOTREACHED() << "Unhandled help source " << source;
  }
  std::unique_ptr<ScopedTabbedBrowserDisplayer> displayer;
  if (!browser) {
    displayer = std::make_unique<ScopedTabbedBrowserDisplayer>(profile);
    browser = displayer->browser();
  }
  ShowSingletonTab(browser, url);
#endif
}

std::string GenerateContentSettingsExceptionsSubPage(ContentSettingsType type) {
  // In MD Settings, the exceptions no longer have a separate subpage.
  // This list overrides the group names defined in site_settings_helper for the
  // purposes of URL generation for MD Settings only. We need this because some
  // of the old group names are no longer appropriate.
  //
  // TODO(crbug.com/728353): Update the group names defined in
  // site_settings_helper once Options is removed from Chrome. Then this list
  // will no longer be needed.

  static constexpr auto kSettingsPathOverrides =
      base::MakeFixedFlatMap<ContentSettingsType, base::StringPiece>(
          {{ContentSettingsType::AUTOMATIC_DOWNLOADS, "automaticDownloads"},
           {ContentSettingsType::BACKGROUND_SYNC, "backgroundSync"},
           {ContentSettingsType::MEDIASTREAM_MIC, "microphone"},
           {ContentSettingsType::MEDIASTREAM_CAMERA, "camera"},
           {ContentSettingsType::MIDI_SYSEX, "midiDevices"},
           {ContentSettingsType::ADS, "ads"},
           {ContentSettingsType::PPAPI_BROKER, "unsandboxedPlugins"}});
  const auto* it = kSettingsPathOverrides.find(type);

  return base::StrCat({kContentSettingsSubPage, "/",
                       (it == kSettingsPathOverrides.end())
                           ? site_settings::ContentSettingsTypeToGroupName(type)
                           : it->second});
}

void ShowSiteSettingsImpl(Browser* browser, Profile* profile, const GURL& url) {
  // If a valid non-file origin, open a settings page specific to the current
  // origin of the page. Otherwise, open Content Settings.
  url::Origin site_origin = url::Origin::Create(url);
  std::string link_destination(chrome::kChromeUIContentSettingsURL);
  // TODO(https://crbug.com/444047): Site Details should work with file:// urls
  // when this bug is fixed, so add it to the allowlist when that happens.
  if (!site_origin.opaque() && (url.SchemeIsHTTPOrHTTPS() ||
                                url.SchemeIs(extensions::kExtensionScheme))) {
    std::string origin_string = site_origin.Serialize();
    url::RawCanonOutputT<char> percent_encoded_origin;
    url::EncodeURIComponent(origin_string.c_str(), origin_string.length(),
                            &percent_encoded_origin);
    link_destination = chrome::kChromeUISiteDetailsPrefixURL +
                       std::string(percent_encoded_origin.data(),
                                   percent_encoded_origin.length());
  }
  NavigateParams params(profile, GURL(link_destination),
                        ui::PAGE_TRANSITION_TYPED);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.browser = browser;
  Navigate(&params);
}

}  // namespace

void ShowBookmarkManager(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  ShowSingletonTabIgnorePathOverwriteNTP(browser, GURL(kChromeUIBookmarksURL));
}

void ShowBookmarkManagerForNode(Browser* browser, int64_t node_id) {
  base::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  OpenBookmarkManagerForNode(browser, node_id);
}

void ShowHistory(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowHistory"));
  ShowSingletonTabIgnorePathOverwriteNTP(browser, GURL(kChromeUIHistoryURL));
}

void ShowDownloads(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowDownloads"));
  if (browser->window() && browser->window()->IsDownloadShelfVisible())
    browser->window()->GetDownloadShelf()->Close();

  NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIDownloadsURL)));
  ShowSingletonTabOverwritingNTP(browser, &params);
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
  ShowHelpImpl(NULL, profile, source);
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void ShowChromeTips(Browser* browser) {
  static const char kChromeTipsURL[] = "https://www.google.com/chrome/tips/";
  ShowSingletonTab(browser, GURL(kChromeTipsURL));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

void LaunchReleaseNotes(Profile* profile, apps::mojom::LaunchSource source) {
#if BUILDFLAG(IS_CHROMEOS_ASH) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  LaunchReleaseNotesImpl(profile, source);
#endif
}

void ShowBetaForum(Browser* browser) {
  ShowSingletonTab(browser, GURL(kChromeBetaForumURL));
}

void ShowPolicy(Browser* browser) {
  ShowSingletonTab(browser, GURL(kChromeUIPolicyURL));
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
  ShowSettingsSubPage(browser, kPasswordManagerSubPage);
}

void ShowPasswordCheck(Browser* browser) {
  base::RecordAction(UserMetricsAction("Options_ShowPasswordCheck"));
  ShowSettingsSubPage(browser, kPasswordCheckSubPage);
}

void ShowSafeBrowsingEnhancedProtection(Browser* browser) {
  base::RecordAction(
      UserMetricsAction("Options_ShowSafeBrowsingEnhancedProtection"));
  ShowSettingsSubPage(browser, kSafeBrowsingEnhancedProtectionSubPage);
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ShowEnterpriseManagementPageInTabbedBrowser(Browser* browser) {
  // Management shows in a tab because it has a "back" arrow that takes the
  // user to the Chrome browser about page, which is part of browser settings.
  ShowSingletonTabIgnorePathOverwriteNTP(browser, GURL(kChromeUIManagementURL));
}

void ShowAppManagementPage(Profile* profile,
                           const std::string& app_id,
                           AppManagementEntryPoint entry_point) {
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

void ShowPrintManagementApp(Profile* profile) {
  LaunchSystemWebAppAsync(profile, web_app::SystemAppType::PRINT_MANAGEMENT);
}

void ShowConnectivityDiagnosticsApp(Profile* profile) {
  LaunchSystemWebAppAsync(profile,
                          web_app::SystemAppType::CONNECTIVITY_DIAGNOSTICS);
}

void ShowScanningApp(Profile* profile) {
  LaunchSystemWebAppAsync(profile, web_app::SystemAppType::SCANNING);
}

void ShowDiagnosticsApp(Profile* profile) {
  DCHECK(base::FeatureList::IsEnabled(chromeos::features::kDiagnosticsApp));

  LaunchSystemWebAppAsync(profile, web_app::SystemAppType::DIAGNOSTICS);
}

GURL GetOSSettingsUrl(const std::string& sub_page) {
  DCHECK(sub_page.empty() || chromeos::settings::IsOSSettingsSubPage(sub_page))
      << sub_page;
  std::string url = kChromeUIOSSettingsURL;
  return GURL(url + sub_page);
}
#endif

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
void ShowBrowserSignin(Browser* browser,
                       signin_metrics::AccessPoint access_point,
                       signin::ConsentLevel consent_level) {
  Profile* original_profile = browser->profile()->GetOriginalProfile();
  DCHECK(original_profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));

  // If the browser's profile is an incognito profile, make sure to use
  // a browser window from the original profile. The user cannot sign in
  // from an incognito window.
  auto displayer =
      std::make_unique<ScopedTabbedBrowserDisplayer>(original_profile);
  browser = displayer->browser();

  profiles::BubbleViewMode bubble_view_mode;
  if (IdentityManagerFactory::GetForProfile(original_profile)
          ->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    bubble_view_mode = profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH;
  } else {
    switch (consent_level) {
      case signin::ConsentLevel::kSync:
        bubble_view_mode = profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN;
        break;
      case signin::ConsentLevel::kSignin:
        bubble_view_mode = profiles::BUBBLE_VIEW_MODE_GAIA_ADD_ACCOUNT;
        break;
    }
  }

  browser->signin_view_controller()->ShowSignin(bubble_view_mode, access_point);
}

void ShowBrowserSigninOrSettings(Browser* browser,
                                 signin_metrics::AccessPoint access_point) {
  Profile* original_profile = browser->profile()->GetOriginalProfile();
  DCHECK(original_profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
  if (IdentityManagerFactory::GetForProfile(original_profile)
          ->HasPrimaryAccount(signin::ConsentLevel::kSync))
    ShowSettings(browser);
  else
    ShowBrowserSignin(browser, access_point, signin::ConsentLevel::kSync);
}
#endif

}  // namespace chrome
