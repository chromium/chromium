// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROME_PAGES_H_
#define CHROME_BROWSER_UI_CHROME_PAGES_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/feedback/public/feedback_source.h"
#include "chrome/browser/ui/user_education/show_promo_in_page.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/signin/signin_promo.h"
// Removed after browser_finder.h migrate.
#include "chrome/browser/ui/browser.h"
#endif

namespace apps {
enum class LaunchSource;
}

namespace safe_browsing {
enum class SafeBrowsingSettingReferralMethod;
}

namespace signin {
enum class ConsentLevel;
}  // namespace signin

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
namespace web_app {
enum class AppSettingsPageEntryPoint;
}  // namespace web_app
#endif

class BrowserWindowInterface;
class Profile;

namespace chrome {

// Sources of requests to show the help tab.
enum class HelpSource {
  // Keyboard accelerators.
  kKeyboard,

  // Menus (e.g. app menu or Chrome OS system menu).
  kMenu,

  // WebHID help center article.
  kWebHID,

  // WebUI (the "About" page).
  kWebUI,

#if BUILDFLAG(IS_CHROMEOS)
  // WebUI (the OS "About" page).
  kWebUIChromeOS,
#endif

  // WebUSB help center article.
  kWebUSD,
};

void ShowBookmarkManager(BrowserWindowInterface* browser);
void ShowBookmarkManagerForNode(BrowserWindowInterface* browser,
                                int64_t node_id);
void ShowHistory(BrowserWindowInterface* browser, const std::string& host_name);
void ShowHistory(BrowserWindowInterface* browser);
void ShowHistorySubPage(BrowserWindowInterface* browser,
                        std::string_view sub_page);
void ShowDownloads(BrowserWindowInterface* browser);
void ShowExtensions(BrowserWindowInterface* browser,
                    const std::string& extension_to_highlight = std::string());

void ShowHelp(BrowserWindowInterface* browser, HelpSource source);
void ShowHelpForProfile(Profile* profile, HelpSource source);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void ShowChromeTips(BrowserWindowInterface* browser);
void ShowChromeWhatsNew(BrowserWindowInterface* browser);
#endif
void LaunchReleaseNotes(Profile* profile, apps::LaunchSource source);
void ShowBetaForum(BrowserWindowInterface* browser);
void ShowSlow(BrowserWindowInterface* browser);

// Constructs a settings GURL for the specified |sub_page|.
GURL GetSettingsUrl(std::string_view sub_page);

// Constructs a history GURL for the specified `sub_page`.
GURL GetHistoryUrl(std::string_view sub_page);

// Returns true if |browser| is a trusted popup window containing a page with
// matching |scheme| (or any trusted popup if |scheme| is empty).
bool IsTrustedPopupWindowWithScheme(const BrowserWindowInterface* browser,
                                    const std::string& scheme);

// Various things that open in a settings UI.
// NOTE: For Chrome OS settings, use SettingsWindowManager::ShowOSSettings().
void ShowSettings(BrowserWindowInterface* browser);
void ShowSettingsSubPage(BrowserWindowInterface* browser,
                         std::string_view sub_page);
void ShowSettingsSubPageForProfile(Profile* profile, std::string_view sub_page);
void ShowPageWithPromoForProfile(Profile* profile,
                                 ShowPromoInPage::Params promo_params);
void ShowContentSettingsExceptions(BrowserWindowInterface* browser,
                                   ContentSettingsType content_settings_type);
void ShowContentSettingsExceptionsForProfile(
    Profile* profile,
    ContentSettingsType content_settings_type);

void ShowSiteSettings(Profile* profile, const GURL& url);
void ShowSiteSettings(BrowserWindowInterface* browser, const GURL& url);

void ShowSiteSettingsFileSystem(Profile* profile, const GURL& url);
void ShowSiteSettingsFileSystem(BrowserWindowInterface* browser,
                                const GURL& url);

void ShowContentSettings(BrowserWindowInterface* browser,
                         ContentSettingsType content_settings_type);
void ShowSettingsSubPageInTabbedBrowser(BrowserWindowInterface* browser,
                                        std::string_view sub_page);
void ShowClearBrowsingDataDialog(BrowserWindowInterface* browser);
void ShowPasswordManager(BrowserWindowInterface* bwi);
void ShowPasswordManagerSettings(BrowserWindowInterface* bwi);
void ShowPasswordDetailsPage(BrowserWindowInterface* browser,
                             const std::string& password_domain_name);
void ShowPasswordCheck(BrowserWindowInterface* browser);
void ShowSafeBrowsingEnhancedProtection(BrowserWindowInterface* browser);
void ShowSafeBrowsingEnhancedProtectionWithIph(
    BrowserWindowInterface* browser,
    safe_browsing::SafeBrowsingSettingReferralMethod referral_method);
void ShowImportDialog(BrowserWindowInterface* browser);
void ShowAboutChrome(BrowserWindowInterface* browser);
void ShowSearchEngineSettings(BrowserWindowInterface* browser);
void ShowWebStore(BrowserWindowInterface* browser,
                  std::string_view utm_source_value);
void ShowPrivacySandboxSettings(BrowserWindowInterface* browser);
void ShowPrivacySandboxAdMeasurementSettings(BrowserWindowInterface* browser);
void ShowAddresses(BrowserWindowInterface* bwi);
void ShowPaymentMethods(BrowserWindowInterface* bwi);
void ShowContactInfo(BrowserWindowInterface* bwi);
void ShowIdentityDocs(BrowserWindowInterface* bwi);
void ShowTravel(BrowserWindowInterface* bwi);
void ShowAllSitesSettingsFilteredByRwsOwner(
    BrowserWindowInterface* browser,
    const std::string& rws_owner_host_name);

// Shows all recent shared tab group activities.
void ShowSharedTabGroupActivity(Profile* profile);

// Shows the enterprise management info page in a browser tab.
void ShowEnterpriseManagementPageInTabbedBrowser(
    BrowserWindowInterface* browser);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
// Show chrome://app-settings/<app-id> page.
void ShowWebAppSettings(BrowserWindowInterface* browser,
                        const std::string& app_id,
                        web_app::AppSettingsPageEntryPoint entry_point);
void ShowWebAppSettings(Profile* profile,
                        const std::string& app_id,
                        web_app::AppSettingsPageEntryPoint entry_point);
#endif

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_CHROME_PAGES_H_
