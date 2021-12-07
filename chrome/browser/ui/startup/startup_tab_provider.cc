// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab_provider.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/branding_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/util.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/child_process_security_policy.h"
#include "net/base/url_util.h"

#if defined(OS_WIN)
#include "base/strings/string_util_win.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/shell_integration.h"
#endif  // defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace {

// Attempts to find an existing, non-empty tabbed browser for this profile.
bool ProfileHasOtherTabbedBrowser(Profile* profile) {
  BrowserList* browser_list = BrowserList::GetInstance();
  auto other_tabbed_browser = std::find_if(
      browser_list->begin(), browser_list->end(), [profile](Browser* browser) {
        return browser->profile() == profile && browser->is_type_normal() &&
               !browser->tab_strip_model()->empty();
      });
  return other_tabbed_browser != browser_list->end();
}

// Validates the URL whether it is allowed to be opened at launching.
bool ValidateUrl(const GURL& url) {
  // Exclude dangerous schemes.
  if (!url.is_valid())
    return false;

  const GURL settings_url(chrome::kChromeUISettingsURL);
  bool url_points_to_an_approved_settings_page = false;
#if defined(OS_CHROMEOS)
  // In ChromeOS, allow any settings page to be specified on the command line.
  url_points_to_an_approved_settings_page =
      url.DeprecatedGetOriginAsURL() == settings_url.DeprecatedGetOriginAsURL();
#else
  // Exposed for external cleaners to offer a settings reset to the
  // user. The allowed URLs must match exactly.
  const GURL reset_settings_url =
      settings_url.Resolve(chrome::kResetProfileSettingsSubPage);
  url_points_to_an_approved_settings_page = url == reset_settings_url;
#if defined(OS_WIN)
  // On Windows, also allow a hash for the Chrome Cleanup Tool.
  const GURL reset_settings_url_with_cct_hash = reset_settings_url.Resolve(
      std::string("#") + settings::ResetSettingsHandler::kCctResetSettingsHash);
  url_points_to_an_approved_settings_page =
      url_points_to_an_approved_settings_page ||
      url == reset_settings_url_with_cct_hash;
#endif  // defined(OS_WIN)
#endif  // defined(OS_CHROMEOS)

  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  return policy->IsWebSafeScheme(url.scheme()) ||
         url.SchemeIs(url::kFileScheme) ||
         url_points_to_an_approved_settings_page ||
         url.spec() == url::kAboutBlankURL;
}

}  // namespace

StartupTabs StartupTabProviderImpl::GetOnboardingTabs(Profile* profile) const {
// Chrome OS has its own welcome flow provided by OOBE.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return StartupTabs();
#else
  if (!profile)
    return StartupTabs();

  StandardOnboardingTabsParams standard_params;
  standard_params.is_first_run = first_run::IsChromeFirstRun();
  PrefService* prefs = profile->GetPrefs();
  standard_params.has_seen_welcome_page =
      prefs && prefs->GetBoolean(prefs::kHasSeenWelcomePage);
  standard_params.is_signin_allowed =
      SyncServiceFactory::IsSyncAllowed(profile);
  if (auto* identity_manager = IdentityManagerFactory::GetForProfile(profile)) {
    standard_params.is_signed_in =
        identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync);
  }
  standard_params.is_child_account = profile->IsChild();
  standard_params.is_force_signin_enabled = signin_util::IsForceSigninEnabled();

  return GetStandardOnboardingTabsForState(standard_params);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if defined(OS_WIN)
StartupTabs StartupTabProviderImpl::GetWelcomeBackTabs(
    Profile* profile,
    StartupBrowserCreator* browser_creator,
    chrome::startup::IsProcessStartup process_startup) const {
  StartupTabs tabs;
  if (process_startup == chrome::startup::IsProcessStartup::kNo ||
      !browser_creator) {
    return tabs;
  }
  if (browser_creator->welcome_back_page() &&
      CanShowWelcome(SyncServiceFactory::IsSyncAllowed(profile),
                     profile->IsChild(), signin_util::IsForceSigninEnabled())) {
    tabs.emplace_back(GetWelcomePageUrl(false));
  }
  return tabs;
}
#endif  // defined(OS_WIN)

StartupTabs StartupTabProviderImpl::GetDistributionFirstRunTabs(
    StartupBrowserCreator* browser_creator) const {
  if (!browser_creator)
    return StartupTabs();
  StartupTabs tabs = GetInitialPrefsTabsForState(
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

StartupTabs StartupTabProviderImpl::GetCommandLineTabs(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile) const {
  StartupTabs result;

  for (const auto& arg : command_line.GetArgs()) {
    ParsedCommandLineTabArg parsed_arg =
        ParseTabFromCommandLineArg(arg, cur_dir, profile);

    // `ParseTabFromCommandLineArg()` shouldn't return
    // CommandLineTabsPresent::kUnknown when a profile is provided.
    DCHECK_NE(parsed_arg.tab_parsed, CommandLineTabsPresent::kUnknown);

    if (parsed_arg.tab_parsed == CommandLineTabsPresent::kYes) {
      result.emplace_back(std::move(parsed_arg.tab_url));
    }
  }

  return result;
}

CommandLineTabsPresent StartupTabProviderImpl::HasCommandLineTabs(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir) const {
  bool is_unknown = false;
  for (const auto& arg : command_line.GetArgs()) {
    ParsedCommandLineTabArg parsed_arg =
        ParseTabFromCommandLineArg(arg, cur_dir, /*maybe_profile=*/nullptr);
    if (parsed_arg.tab_parsed == CommandLineTabsPresent::kYes) {
      return CommandLineTabsPresent::kYes;
    }
    if (parsed_arg.tab_parsed == CommandLineTabsPresent::kUnknown) {
      is_unknown = true;
    }
  }

  return is_unknown ? CommandLineTabsPresent::kUnknown
                    : CommandLineTabsPresent::kNo;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
StartupTabs StartupTabProviderImpl::GetCrosapiTabs() const {
  auto* init_params = chromeos::LacrosService::Get()->init_params();
  if (init_params->initial_browser_action !=
          crosapi::mojom::InitialBrowserAction::kOpenWindowWithUrls ||
      !init_params->startup_urls.has_value()) {
    return {};
  }

  StartupTabs result;
  for (const GURL& url : *init_params->startup_urls) {
    if (ValidateUrl(url))
      result.emplace_back(url);
  }
  return result;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if !defined(OS_ANDROID)
StartupTabs StartupTabProviderImpl::GetNewFeaturesTabs(
    bool whats_new_enabled) const {
  return GetNewFeaturesTabsForState(whats_new_enabled);
}
#endif  // !defined(OS_ANDROID)

// static
bool StartupTabProviderImpl::CanShowWelcome(bool is_signin_allowed,
                                            bool is_child_account,
                                            bool is_force_signin_enabled) {
  return is_signin_allowed && !is_child_account && !is_force_signin_enabled;
}

// static
bool StartupTabProviderImpl::ShouldShowWelcomeForOnboarding(
    bool has_seen_welcome_page,
    bool is_signed_in) {
  return !has_seen_welcome_page && !is_signed_in;
}

// static
StartupTabs StartupTabProviderImpl::GetStandardOnboardingTabsForState(
    const StandardOnboardingTabsParams& params) {
  StartupTabs tabs;
  if (CanShowWelcome(params.is_signin_allowed, params.is_child_account,
                     params.is_force_signin_enabled) &&
      ShouldShowWelcomeForOnboarding(params.has_seen_welcome_page,
                                     params.is_signed_in)) {
    tabs.emplace_back(GetWelcomePageUrl(!params.is_first_run));
  }
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetInitialPrefsTabsForState(
    bool is_first_run,
    const std::vector<GURL>& first_run_tabs) {
  // Constants: Magic words used by initial preferences files in place of a URL
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
      tabs.emplace_back(url);
    }
  }
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetResetTriggerTabsForState(
    bool profile_has_trigger) {
  StartupTabs tabs;
  if (profile_has_trigger)
    tabs.emplace_back(GetTriggeredResetSettingsUrl());
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetPinnedTabsForState(
    const SessionStartupPref& pref,
    const StartupTabs& pinned_tabs,
    bool profile_has_other_tabbed_browser) {
  if (pref.ShouldRestoreLastSession() || profile_has_other_tabbed_browser)
    return StartupTabs();
  return pinned_tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetPreferencesTabsForState(
    const SessionStartupPref& pref,
    bool profile_has_other_tabbed_browser) {
  StartupTabs tabs;
  if (pref.ShouldOpenUrls() && !pref.urls.empty() &&
      !profile_has_other_tabbed_browser) {
    for (const auto& url : pref.urls)
      tabs.emplace_back(url);
  }
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetNewTabPageTabsForState(
    const SessionStartupPref& pref) {
  StartupTabs tabs;
  if (!pref.ShouldRestoreLastSession())
    tabs.emplace_back(GURL(chrome::kChromeUINewTabURL));
  return tabs;
}

// static
StartupTabs StartupTabProviderImpl::GetPostCrashTabsForState(
    bool has_incompatible_applications) {
  StartupTabs tabs;
  if (has_incompatible_applications)
    AddIncompatibleApplicationsUrl(&tabs);
  return tabs;
}

#if !defined(OS_ANDROID)
// static
StartupTabs StartupTabProviderImpl::GetNewFeaturesTabsForState(
    bool whats_new_enabled) {
  StartupTabs tabs;
  if (whats_new_enabled)
    tabs.emplace_back(whats_new::GetWebUIStartupURL());
  return tabs;
}
#endif

// static
GURL StartupTabProviderImpl::GetWelcomePageUrl(bool use_later_run_variant) {
  GURL url(chrome::kChromeUIWelcomeURL);
  return use_later_run_variant
             ? net::AppendQueryParameter(url, "variant", "everywhere")
             : url;
}

// static
void StartupTabProviderImpl::AddIncompatibleApplicationsUrl(StartupTabs* tabs) {
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  UMA_HISTOGRAM_BOOLEAN("IncompatibleApplicationsPage.AddedPostCrash", true);
  GURL url(chrome::kChromeUISettingsURL);
  tabs->emplace_back(url.Resolve("incompatibleApplications"));
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// static
GURL StartupTabProviderImpl::GetTriggeredResetSettingsUrl() {
  return GURL(
      chrome::GetSettingsUrl(chrome::kTriggeredResetProfileSettingsSubPage));
}

// static
StartupTabProviderImpl::ParsedCommandLineTabArg
StartupTabProviderImpl::ParseTabFromCommandLineArg(
    base::FilePath::StringPieceType arg,
    const base::FilePath& cur_dir,
    Profile* maybe_profile) {
  // Note: Type/encoding of |arg| matches with the one of FilePath.
  // So, we use them for encoding conversions.

  // Handle Vista way of searching - "? <search-term>"
  if (base::StartsWith(arg, FILE_PATH_LITERAL("? "))) {
    if (maybe_profile == nullptr) {
      // In the absence of profile, we are not able to resolve the search URL.
      // We indicate that we don't know whether a tab would be successfully
      // created or not.
      return {CommandLineTabsPresent::kUnknown, GURL()};
    }

    GURL url(GetDefaultSearchURLForSearchTerms(
        TemplateURLServiceFactory::GetForProfile(maybe_profile),
        base::FilePath(arg).LossyDisplayName().substr(/* remove "? " */ 2)));
    if (url.is_valid()) {
      return {CommandLineTabsPresent::kYes, std::move(url)};
    }
  } else {
    // Otherwise, fall through to treating it as a URL.
    // This will create a file URL or a regular URL.
    GURL url(base::FilePath(arg).MaybeAsASCII());

    // This call can (in rare circumstances) block the UI thread.
    // FixupRelativeFile may access to current working directory, which is a
    // blocking API. http://crbug.com/60641
    // http://crbug.com/371030: Only use URLFixerUpper if we don't have a valid
    // URL, otherwise we will look in the current directory for a file named
    // 'about' if the browser was started with a about:foo argument.
    // http://crbug.com/424991: Always use URLFixerUpper on file:// URLs,
    // otherwise we wouldn't correctly handle '#' in a file name.
    if (!url.is_valid() || url.SchemeIsFile()) {
      base::ScopedAllowBlocking allow_blocking;
      url = url_formatter::FixupRelativeFile(cur_dir, base::FilePath(arg));
    }

    if (ValidateUrl(url))
      return {CommandLineTabsPresent::kYes, std::move(url)};
  }

  return {CommandLineTabsPresent::kNo, GURL()};
}
