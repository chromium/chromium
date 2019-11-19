// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/chrome_pages.h"

#include <stddef.h>

#include <memory>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/apps/launch_service/launch_service.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/chromeos/extensions/default_web_app_ids.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/bookmarks/bookmarks_ui.h"
#include "chrome/browser/ui/webui/site_settings_helper.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/system/statistics_provider.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/url_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/extension_registry.h"
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

void OpenBookmarkManagerForNode(Browser* browser, int64_t node_id) {
  GURL url = GURL(kChromeUIBookmarksURL)
                 .Resolve(base::StringPrintf(
                     "/?id=%s", base::NumberToString(node_id).c_str()));
  NavigateParams params(GetSingletonTabNavigateParams(browser, url));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, std::move(params));
}

#if defined(OS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

const std::string BuildQueryString(Profile* profile) {
  const std::string board_name = base::SysInfo::GetLsbReleaseBoard();
  std::string region;
  chromeos::system::StatisticsProvider::GetInstance()->GetMachineStatistic(
      "region", &region);
  const std::string language = l10n_util::GetApplicationLocale(std::string());
  const std::string version = version_info::GetVersionNumber();
  const std::string milestone = version_info::GetMajorVersionNumber();
  std::string channel_name =
      chrome::GetChannelName();  // beta, dev, canary, unknown, or empty string
                                 // for stable
  if (channel_name.empty())
    channel_name = "stable";
  const std::string username = profile->GetProfileUserName();
  std::string user_type;
  if (gaia::IsGoogleInternalAccountEmail(username)) {
    user_type = "googler";
  } else if (profile->GetProfilePolicyConnector()->IsManaged()) {
    user_type = "managed";
  } else {
    user_type = "general";
  }

  const std::string query_string = base::StrCat(
      {kChromeReleaseNotesURL, "?version=", milestone, "&tags=", board_name,
       ",", region, ",", language, ",", channel_name, ",", user_type});
  return query_string;
}

void LaunchReleaseNotesInTab(Profile* profile) {
  GURL url(BuildQueryString(profile));
  auto displayer = std::make_unique<ScopedTabbedBrowserDisplayer>(profile);
  ShowSingletonTab(displayer->browser(), url);
}

void LaunchReleaseNotesImpl(Profile* profile) {
  base::RecordAction(UserMetricsAction("ReleaseNotes.ShowReleaseNotes"));
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          chromeos::default_web_apps::kReleaseNotesAppId,
          extensions::ExtensionRegistry::EVERYTHING);
  if (extension) {
    apps::AppLaunchParams params = CreateAppLaunchParamsWithEventFlags(
        profile, extension, 0, apps::mojom::AppLaunchSource::kSourceUntracked,
        -1);
    params.override_url = GURL(BuildQueryString(profile));
    apps::LaunchService::Get(profile)->OpenApplication(params);
    return;
  }
  DVLOG(1) << "ReleaseNotes App Not Found";
  LaunchReleaseNotesInTab(profile);
}

#endif

// Shows either the help app or the appropriate help page for |source|. If
// |browser| is NULL and the help page is used (vs the app), the help page is
// shown in the last active browser. If there is no such browser, a new browser
// is created.
void ShowHelpImpl(Browser* browser, Profile* profile, HelpSource source) {
  base::RecordAction(UserMetricsAction("ShowHelpTab"));
#if defined(OS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetExtensionById(
          extension_misc::kGeniusAppId,
          extensions::ExtensionRegistry::EVERYTHING);
  if (!extension) {
    DCHECK(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableDefaultApps));
    return;
  }
  extensions::AppLaunchSource app_launch_source(
      extensions::AppLaunchSource::kSourceUntracked);
  switch (source) {
    case HELP_SOURCE_KEYBOARD:
      app_launch_source = extensions::AppLaunchSource::kSourceKeyboard;
      break;
    case HELP_SOURCE_MENU:
      app_launch_source = extensions::AppLaunchSource::kSourceSystemTray;
      break;
    case HELP_SOURCE_WEBUI:
    case HELP_SOURCE_WEBUI_CHROME_OS:
      app_launch_source = extensions::AppLaunchSource::kSourceAboutPage;
      break;
    default:
      NOTREACHED() << "Unhandled help source" << source;
  }
  apps::LaunchService::Get(profile)->OpenApplication(apps::AppLaunchParams(
      extension_misc::kGeniusAppId,
      extensions::GetLaunchContainer(extensions::ExtensionPrefs::Get(profile),
                                     extension),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, app_launch_source, true));
#else
  GURL url;
  switch (source) {
    case HELP_SOURCE_KEYBOARD:
      url = GURL(kChromeHelpViaKeyboardURL);
      break;
    case HELP_SOURCE_MENU:
      url = GURL(kChromeHelpViaMenuURL);
      break;
#if defined(OS_CHROMEOS)
    case HELP_SOURCE_WEBUI:
      if (base::FeatureList::IsEnabled(chromeos::features::kSplitSettings))
        url = GURL(kChromeHelpViaWebUIURL);
      else
        url = GURL(kChromeOsHelpViaWebUIURL);
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
  // of the old group names are no longer appropriate: i.e. "plugins" =>
  // "flash".
  //
  // TODO(crbug.com/728353): Update the group names defined in
  // site_settings_helper once Options is removed from Chrome. Then this list
  // will no longer be needed.
  static base::NoDestructor<std::map<ContentSettingsType, std::string>>
      kSettingsPathOverrides(
          {{ContentSettingsType::AUTOMATIC_DOWNLOADS, "automaticDownloads"},
           {ContentSettingsType::BACKGROUND_SYNC, "backgroundSync"},
           {ContentSettingsType::MEDIASTREAM_MIC, "microphone"},
           {ContentSettingsType::MEDIASTREAM_CAMERA, "camera"},
           {ContentSettingsType::MIDI_SYSEX, "midiDevices"},
           {ContentSettingsType::PLUGINS, "flash"},
           {ContentSettingsType::ADS, "ads"},
           {ContentSettingsType::PPAPI_BROKER, "unsandboxedPlugins"}});
  const auto it = kSettingsPathOverrides->find(type);
  const std::string content_type_path =
      (it == kSettingsPathOverrides->end())
          ? site_settings::ContentSettingsTypeToGroupName(type)
          : it->second;

  return std::string(kContentSettingsSubPage) + "/" + content_type_path;
}

void ShowSiteSettingsImpl(Browser* browser, Profile* profile, const GURL& url) {
  // If a valid non-file origin, open a settings page specific to the current
  // origin of the page. Otherwise, open Content Settings.
  url::Origin site_origin = url::Origin::Create(url);
  std::string link_destination(chrome::kChromeUIContentSettingsURL);
  // TODO(https://crbug.com/444047): Site Details should work with file:// urls
  // when this bug is fixed, so add it to the whitelist when that happens.
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
  NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIBookmarksURL)));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, std::move(params));
}

void ShowBookmarkManagerForNode(Browser* browser, int64_t node_id) {
  base::RecordAction(UserMetricsAction("ShowBookmarkManager"));
  OpenBookmarkManagerForNode(browser, node_id);
}

void ShowHistory(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowHistory"));
  NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIHistoryURL)));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, std::move(params));
}

void ShowDownloads(Browser* browser) {
  base::RecordAction(UserMetricsAction("ShowDownloads"));
  if (browser->window() && browser->window()->IsDownloadShelfVisible())
    browser->window()->GetDownloadShelf()->Close(DownloadShelf::USER_ACTION);

  ShowSingletonTabOverwritingNTP(
      browser,
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIDownloadsURL)));
}

void ShowExtensions(Browser* browser,
                    const std::string& extension_to_highlight) {
  base::RecordAction(UserMetricsAction("ShowExtensions"));
  NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIExtensionsURL)));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  if (!extension_to_highlight.empty()) {
    GURL::Replacements replacements;
    std::string query("id=");
    query += extension_to_highlight;
    replacements.SetQueryStr(query);
    params.url = params.url.ReplaceComponents(replacements);
  }
  ShowSingletonTabOverwritingNTP(browser, std::move(params));
}

void ShowHelp(Browser* browser, HelpSource source) {
  ShowHelpImpl(browser, browser->profile(), source);
}

void ShowHelpForProfile(Profile* profile, HelpSource source) {
  ShowHelpImpl(NULL, profile, source);
}

void LaunchReleaseNotes(Profile* profile) {
#if defined(OS_CHROMEOS) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  LaunchReleaseNotesImpl(profile);
#endif
}

void ShowBetaForum(Browser* browser) {
  ShowSingletonTab(browser, GURL(kChromeBetaForumURL));
}

void ShowPolicy(Browser* browser) {
  ShowSingletonTab(browser, GURL(kChromeUIPolicyURL));
}

void ShowSlow(Browser* browser) {
#if defined(OS_CHROMEOS)
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
#if defined(OS_CHROMEOS)
  ShowSettingsSubPageForProfile(browser->profile(), sub_page);
#else
  ShowSettingsSubPageInTabbedBrowser(browser, sub_page);
#endif
}

void ShowSettingsSubPageForProfile(Profile* profile,
                                   const std::string& sub_page) {
#if defined(OS_CHROMEOS)
  SettingsWindowManager* settings = SettingsWindowManager::GetInstance();
  if (!base::FeatureList::IsEnabled(chromeos::features::kSplitSettings)) {
    base::RecordAction(base::UserMetricsAction("ShowOptions"));
    settings->ShowChromePageForProfile(profile, GetSettingsUrl(sub_page));
    return;
  }
  // TODO(jamescook): When SplitSettings is close to shipping, change this to
  // a DCHECK that the |sub_page| is not an OS-specific setting.
  if (chrome::IsOSSettingsSubPage(sub_page)) {
    settings->ShowOSSettings(profile, sub_page);
    return;
  }
  // Fall through and open browser settings in a tab.
#endif
  Browser* browser = chrome::FindTabbedBrowser(profile, false);
  if (!browser)
    browser = new Browser(Browser::CreateParams(profile, true));
  ShowSettingsSubPageInTabbedBrowser(browser, sub_page);
}

void ShowSettingsSubPageInTabbedBrowser(Browser* browser,
                                        const std::string& sub_page) {
  base::RecordAction(UserMetricsAction("ShowOptions"));

  // Since the user may be triggering navigation from another UI element such as
  // a menu, ensure the web contents (and therefore the settings page that is
  // about to be shown) is focused. (See crbug/926492 for motivation.)
  FocusWebContents(browser);
  GURL gurl = GetSettingsUrl(sub_page);
  NavigateParams params(GetSingletonTabNavigateParams(browser, gurl));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, std::move(params));
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
      browser,
      kContentSettingsSubPage + std::string(kHashMark) +
          site_settings::ContentSettingsTypeToGroupName(content_settings_type));
}

void ShowClearBrowsingDataDialog(Browser* browser) {
  base::RecordAction(UserMetricsAction("ClearBrowsingData_ShowDlg"));
  ShowSettingsSubPage(browser, kClearBrowserDataSubPage);
}

void ShowPasswordManager(Browser* browser) {
  base::RecordAction(UserMetricsAction("Options_ShowPasswordManager"));
  ShowSettingsSubPage(browser, kPasswordManagerSubPage);
}

void ShowImportDialog(Browser* browser) {
  base::RecordAction(UserMetricsAction("Import_ShowDlg"));
  ShowSettingsSubPage(browser, kImportDataSubPage);
}

void ShowAboutChrome(Browser* browser) {
  base::RecordAction(UserMetricsAction("AboutChrome"));
#if defined(OS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(chromeos::features::kSplitSettings)) {
    SettingsWindowManager::GetInstance()->ShowChromePageForProfile(
        browser->profile(), GURL(kChromeUIHelpURL));
    return;
  }
#endif
  NavigateParams params(
      GetSingletonTabNavigateParams(browser, GURL(kChromeUIHelpURL)));
  params.path_behavior = NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser, std::move(params));
}

void ShowSearchEngineSettings(Browser* browser) {
  base::RecordAction(UserMetricsAction("EditSearchEngines"));
  ShowSettingsSubPage(browser, kSearchEnginesSubPage);
}

#if defined(OS_CHROMEOS)
void ShowManagementPageForProfile(Profile* profile) {
  const std::string page_path = "chrome://management";
  base::RecordAction(base::UserMetricsAction("ShowOptions"));
  SettingsWindowManager::GetInstance()->ShowChromePageForProfile(
      profile, GURL(page_path));
}

void ShowAppManagementPage(Profile* profile, const std::string& app_id) {
  DCHECK(base::FeatureList::IsEnabled(features::kAppManagement));
  std::string sub_page =
      base::StrCat({chrome::kAppManagementDetailSubPage, "?id=", app_id});
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile,
                                                               sub_page);
}

GURL GetOSSettingsUrl(const std::string& sub_page) {
  DCHECK(sub_page.empty() || chrome::IsOSSettingsSubPage(sub_page)) << sub_page;
  std::string url =
      base::FeatureList::IsEnabled(chromeos::features::kSplitSettings)
          ? kChromeUIOSSettingsURL
          : kChromeUISettingsURL;
  return GURL(url + sub_page);
}
#endif

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
void ShowBrowserSignin(Browser* browser,
                       signin_metrics::AccessPoint access_point) {
  Profile* original_profile = browser->profile()->GetOriginalProfile();
  DCHECK(original_profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));

  // If the browser's profile is an incognito profile, make sure to use
  // a browser window from the original profile. The user cannot sign in
  // from an incognito window.
  auto displayer =
      std::make_unique<ScopedTabbedBrowserDisplayer>(original_profile);
  browser = displayer->browser();

  profiles::BubbleViewMode bubble_view_mode =
      IdentityManagerFactory::GetForProfile(original_profile)
              ->HasPrimaryAccount()
          ? profiles::BUBBLE_VIEW_MODE_GAIA_REAUTH
          : profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN;
  browser->signin_view_controller()->ShowSignin(bubble_view_mode, browser,
                                                access_point);
}

void ShowBrowserSigninOrSettings(Browser* browser,
                                 signin_metrics::AccessPoint access_point) {
  Profile* original_profile = browser->profile()->GetOriginalProfile();
  DCHECK(original_profile->GetPrefs()->GetBoolean(prefs::kSigninAllowed));
  if (IdentityManagerFactory::GetForProfile(original_profile)
          ->HasPrimaryAccount())
    ShowSettings(browser);
  else
    ShowBrowserSignin(browser, access_point);
}
#endif

}  // namespace chrome
