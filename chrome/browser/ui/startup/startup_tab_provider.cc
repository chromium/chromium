// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab_provider.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "net/base/url_util.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/shell_integration.h"
#endif  // defined(OS_WIN)

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/ui/webui/welcome/nux/constants.h"
#endif  // defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)

namespace {

#if defined(OS_WIN)
// Returns false if the in-product default browser UX is suppressed by install
// mode (e.g., Chrome Canary) or by policy.
bool SetDefaultBrowserAllowed(PrefService* local_state) {
  if (!shell_integration::CanSetAsDefaultBrowser())
    return false;
  return !local_state ||
         !local_state->IsManagedPreference(
             prefs::kDefaultBrowserSettingEnabled) ||
         local_state->GetBoolean(prefs::kDefaultBrowserSettingEnabled);
}
#endif  // defined(OS_WIN)

// Attempts to find an existing, non-empty tabbed browser for this profile.
bool ProfileHasOtherTabbedBrowser(Profile* profile) {
  BrowserList* browser_list = BrowserList::GetInstance();
  auto other_tabbed_browser = std::find_if(
      browser_list->begin(), browser_list->end(), [profile](Browser* browser) {
        return browser->profile() == profile && browser->is_type_tabbed() &&
               !browser->tab_strip_model()->empty();
      });
  return other_tabbed_browser != browser_list->end();
}

}  // namespace

StartupTabs StartupTabProviderImpl::GetOnboardingTabs(Profile* profile) const {
// Onboarding content has not been launched on Chrome OS.
#if defined(OS_CHROMEOS)
  return StartupTabs();
#else
  if (!profile)
    return StartupTabs();

  StandardOnboardingTabsParams standard_params;
  standard_params.is_first_run = first_run::IsChromeFirstRun();
  PrefService* prefs = profile->GetPrefs();
  standard_params.has_seen_welcome_page =
      prefs && prefs->GetBoolean(prefs::kHasSeenWelcomePage);
  standard_params.is_signin_allowed = profile->IsSyncAllowed();
  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile);
  standard_params.is_signed_in =
      signin_manager && signin_manager->IsAuthenticated();
  standard_params.is_signin_in_progress =
      signin_manager && signin_manager->AuthInProgress();
  standard_params.is_supervised_user = profile->IsSupervised();
  standard_params.is_force_signin_enabled = signin_util::IsForceSigninEnabled();

// TODO(scottchen): make win-10 also show NUX onboarding page when its enabled.

#if defined(OS_WIN)
  // Windows 10 has unique onboarding policies and content.
  if (base::win::GetVersion() >= base::win::VERSION_WIN10) {
    Win10OnboardingTabsParams win10_params;
    PrefService* local_state = g_browser_process->local_state();
    const shell_integration::DefaultWebClientState web_client_state =
        g_browser_process->CachedDefaultWebClientState();
    win10_params.has_seen_win10_promo =
        local_state && local_state->GetBoolean(prefs::kHasSeenWin10PromoPage);
    win10_params.set_default_browser_allowed =
        SetDefaultBrowserAllowed(local_state);
    // Do not welcome if this Chrome or another side-by-side install was the
    // default browser at startup.
    win10_params.is_default_browser =
        web_client_state == shell_integration::IS_DEFAULT ||
        web_client_state == shell_integration::OTHER_MODE_IS_DEFAULT;
    return GetWin10OnboardingTabsForState(standard_params, win10_params);
  }
#endif  // defined(OS_WIN)

  return GetStandardOnboardingTabsForState(standard_params);
#endif  // defined(OS_CHROMEOS)
}

StartupTabs StartupTabProviderImpl::GetWelcomeBackTabs(
    Profile* profile,
    StartupBrowserCreator* browser_creator,
    bool process_startup) const {
  StartupTabs tabs;
  if (!process_startup || !browser_creator)
    return tabs;
  switch (browser_creator->welcome_back_page()) {
    case StartupBrowserCreator::WelcomeBackPage::kNone:
      break;
#if defined(OS_WIN)
    case StartupBrowserCreator::WelcomeBackPage::kWelcomeWin10:
      if (CanShowWin10Welcome(
              SetDefaultBrowserAllowed(g_browser_process->local_state()),
              profile->IsSupervised())) {
        tabs.emplace_back(GetWin10WelcomePageUrl(false), false);
        break;
      }
      FALLTHROUGH;
#endif  // defined(OS_WIN)
    case StartupBrowserCreator::WelcomeBackPage::kWelcomeStandard:
      if (CanShowWelcome(profile->IsSyncAllowed(), profile->IsSupervised(),
                         signin_util::IsForceSigninEnabled())) {
        tabs.emplace_back(GetWelcomePageUrl(false), false);
      }
      break;
  }
  return tabs;
}

StartupTabs StartupTabProviderImpl::GetDistributionFirstRunTabs(
    StartupBrowserCreator* browser_creator) const {
  if (!browser_creator)
    return StartupTabs();
  StartupTabs tabs = GetMasterPrefsTabsForState(
      first_run::IsChromeFirstRun(), browser_creator->first_run_tabs_);
  browser_creator->first_run_tabs_.clear();
  return tabs;
}

StartupTabs StartupTabProviderImpl::GetResetTriggerTabs(
    Profile* profile) const {
  auto* triggered_profile_resetter =
      TriggeredProfileResetterFactory::GetForBrowserContext(profile);
  bool has_reset_trigger = triggered_profile_resetter &&
                           triggered_profile_resetter->HasResetTrigger();
  return GetResetTriggerTabsForState(has_reset_trigger);
}

StartupTabs StartupTabProviderImpl::GetPinnedTabs(
    const base::CommandLine& command_line,
    Profile* profile) const {
  return GetPinnedTabsForState(
      StartupBrowserCreator::GetSessionStartupPref(command_line, profile),
      PinnedTabCodec::ReadPinnedTabs(profile),
      ProfileHasOtherTabbedBrowser(profile));
}

StartupTabs StartupTabProviderImpl::GetPreferencesTabs(
    const base::CommandLine& command_line,
    Profile* profile) const {
  return GetPreferencesTabsForState(
      StartupBrowserCreator::GetSessionStartupPref(command_line, profile),
      ProfileHasOtherTabbedBrowser(profile));
}

StartupTabs StartupTabProviderImpl::GetNewTabPageTabs(
    const base::CommandLine& command_line,
    Profile* profile) const {
  return GetNewTabPageTabsForState(
      StartupBrowserCreator::GetSessionStartupPref(command_line, profile));
}

StartupTabs StartupTabProviderImpl::GetPostCrashTabs(
    bool has_incompatible_applications) const {
  return GetPostCrashTabsForState(has_incompatible_applications);
}

// static
bool StartupTabProviderImpl::CanShowWelcome(bool is_signin_allowed,
                                            bool is_supervised_user,
                                            bool is_force_signin_enabled) {
  return is_signin_allowed && !is_supervised_user && !is_force_signin_enabled;
}

// static
bool StartupTabProviderImpl::ShouldShowWelcomeForOnboarding(
    bool has_seen_welcome_page,
    bool is_signed_in,
    bool is_signin_in_progress) {
  return !has_seen_welcome_page && !is_signed_in && !is_signin_in_progress;
}

// static
StartupTabs StartupTabProviderImpl::GetStandardOnboardingTabsForState(
    const StandardOnboardingTabsParams& params) {
  StartupTabs tabs;
  if (CanShowWelcome(params.is_signin_allowed, params.is_supervised_user,
                     params.is_force_signin_enabled) &&
      ShouldShowWelcomeForOnboarding(params.has_seen_welcome_page,
                                     params.is_signed_in,
                                     params.is_signin_in_progress)) {
    tabs.emplace_back(GetWelcomePageUrl(!params.is_first_run), false);
  }
  return tabs;
}

#if defined(OS_WIN)
// static
bool StartupTabProviderImpl::CanShowWin10Welcome(
    bool set_default_browser_allowed,
    bool is_supervised_user) {
  return set_default_browser_allowed && !is_supervised_user;
}

// static
bool StartupTabProviderImpl::ShouldShowWin10WelcomeForOnboarding(
    bool has_seen_win10_promo,
    bool is_default_browser) {
  return !has_seen_win10_promo && !is_default_browser;
}

// static
StartupTabs StartupTabProviderImpl::GetWin10OnboardingTabsForState(
    const StandardOnboardingTabsParams& standard_params,
    const Win10OnboardingTabsParams& win10_params) {
  if (CanShowWin10Welcome(win10_params.set_default_browser_allowed,
                          standard_params.is_supervised_user) &&
      ShouldShowWin10WelcomeForOnboarding(win10_params.has_seen_win10_promo,
                                          win10_params.is_default_browser)) {
    return {StartupTab(GetWin10WelcomePageUrl(!standard_params.is_first_run),
                       false)};
  }

  return GetStandardOnboardingTabsForState(standard_params);
}
#endif  // defined(OS_WIN)

// static
StartupTabs StartupTabProviderImpl::GetMasterPrefsTabsForState(
    bool is_first_run,
    const std::vector<GURL>& first_run_tabs) {
  // Constants: Magic words used by Master Preferences files in place of a URL
  // host to indicate that internal pages should appear on first run.
  static constexpr char kNewTabUrlHost[] = "new_tab_page";
  static constexpr char kWelcomePageUrlHost[] = "welcome_page";

  StartupTabs tabs;
  if (is_first_run) {
    tabs.reserve(first_run_tabs.size());
    for (GURL url : first_run_tabs) {
      if (url.host_piece() == kNewTabUrlHost)
        url = GURL(chrome::kChromeUINewTabURL);
      else if (url.host_piece() == kWelcomePageUrlHost)
        url = GetWelcomePageUrl(false);
      tabs.emplace_back(url, false);
    }
  }
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetResetTriggerTabsForState(
    bool profile_has_trigger) {
  StartupTabs tabs;
  if (profile_has_trigger)
    tabs.emplace_back(GetTriggeredResetSettingsUrl(), false);
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetPinnedTabsForState(
    const SessionStartupPref& pref,
    const StartupTabs& pinned_tabs,
    bool profile_has_other_tabbed_browser) {
  if (pref.type == SessionStartupPref::Type::LAST ||
      profile_has_other_tabbed_browser)
    return StartupTabs();
  return pinned_tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetPreferencesTabsForState(
    const SessionStartupPref& pref,
    bool profile_has_other_tabbed_browser) {
  StartupTabs tabs;
  if (pref.type == SessionStartupPref::Type::URLS && !pref.urls.empty() &&
      !profile_has_other_tabbed_browser) {
    for (const auto& url : pref.urls)
      tabs.push_back(StartupTab(url, false));
  }
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetNewTabPageTabsForState(
    const SessionStartupPref& pref) {
  StartupTabs tabs;
  if (pref.type != SessionStartupPref::Type::LAST)
    tabs.emplace_back(GURL(chrome::kChromeUINewTabURL), false);
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetPostCrashTabsForState(
    bool has_incompatible_applications) {
  StartupTabs tabs;
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  if (has_incompatible_applications)
    tabs.emplace_back(GetIncompatibleApplicationsUrl(), false);
#endif
  return tabs;
}

// static
GURL StartupTabProviderImpl::GetWelcomePageUrl(bool use_later_run_variant) {
  GURL url(chrome::kChromeUIWelcomeURL);
  return use_later_run_variant
             ? net::AppendQueryParameter(url, "variant", "everywhere")
             : url;
}

#if defined(OS_WIN)
// static
GURL StartupTabProviderImpl::GetWin10WelcomePageUrl(
    bool use_later_run_variant) {
  // Record that the Welcome page was added to the startup url list.
  UMA_HISTOGRAM_BOOLEAN("Welcome.Win10.NewPromoPageAdded", true);
  GURL url(chrome::kChromeUIWelcomeWin10URL);
  return use_later_run_variant
             ? net::AppendQueryParameter(url, "text", "faster")
             : url;
}

#if defined(GOOGLE_CHROME_BUILD)
// static
GURL StartupTabProviderImpl::GetIncompatibleApplicationsUrl() {
  UMA_HISTOGRAM_BOOLEAN("IncompatibleApplicationsPage.AddedPostCrash", true);
  GURL url(chrome::kChromeUISettingsURL);
  return url.Resolve("incompatibleApplications");
}
#endif  // defined(GOOGLE_CHROME_BUILD)
#endif  // defined(OS_WIN)

// static
GURL StartupTabProviderImpl::GetTriggeredResetSettingsUrl() {
  return GURL(
      chrome::GetSettingsUrl(chrome::kTriggeredResetProfileSettingsSubPage));
}
