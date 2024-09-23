// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/install_chrome_app.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/headless/headless_command_processor.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/infobar_utils.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_tab.h"
#include "chrome/browser/ui/startup/startup_tab_provider.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/app_controller_mac.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/win/conflicts/incompatible_applications_updater.h"
#endif

#if BUILDFLAG(ENABLE_RLZ)
#include "components/google/core/common/google_util.h"
#include "components/rlz/rlz_tracker.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/app_restore/full_restore_utils.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/chromeos/arc/arc_web_contents_data.h"
#include "chrome/browser/lacros/browser_launcher.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/webui/whats_new/whats_new_fetcher.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

namespace {

// Utility functions ----------------------------------------------------------

// On ChromeOS Ash check the previous apps launching history info to decide
// whether restore apps.
//
// On ChromeOS Lacros restore if the browser has automatically restarted or if
// performing a full restore.
//
// In other platforms, restore apps only when the browser is automatically
// restarted.
bool ShouldRestoreApps(bool is_post_restart, Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // In ChromeOS, restore apps only when there are apps launched before reboot.
  return full_restore::HasAppTypeBrowser(profile->GetPath());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* primary_user_profile =
      g_browser_process->profile_manager()->GetProfileByPath(
          ProfileManager::GetPrimaryUserProfilePath());

  return is_post_restart ||
         (primary_user_profile &&
          BrowserLauncher::GetForProfile(primary_user_profile)
              ->is_launching_for_last_opened_profiles());
#else
  return is_post_restart;
#endif
}

void UrlsToTabs(const std::vector<GURL>& urls, StartupTabs* tabs) {
  for (const GURL& url : urls)
    tabs->emplace_back(url);
}

// Appends the contents of |from| to the end of |to|.
void AppendTabs(const StartupTabs& from, StartupTabs* to) {
  to->insert(to->end(), from.begin(), from.end());
}

// Prepends the contents of |from| to the beginning of |to|.
void PrependTabs(const StartupTabs& from, StartupTabs* to) {
  to->insert(to->begin(), from.begin(), from.end());
}

}  // namespace

StartupBrowserCreatorImpl::StartupBrowserCreatorImpl(
    const base::FilePath& cur_dir,
    const base::CommandLine& command_line,
    chrome::startup::IsFirstRun is_first_run)
    : cur_dir_(cur_dir),
      command_line_(command_line),
      profile_(nullptr),
      browser_creator_(nullptr),
      is_first_run_(is_first_run) {}

StartupBrowserCreatorImpl::StartupBrowserCreatorImpl(
    const base::FilePath& cur_dir,
    const base::CommandLine& command_line,
    StartupBrowserCreator* browser_creator,
    chrome::startup::IsFirstRun is_first_run)
    : cur_dir_(cur_dir),
      command_line_(command_line),
      browser_creator_(browser_creator),
      is_first_run_(is_first_run) {}

// static
void StartupBrowserCreatorImpl::MaybeToggleFullscreen(Browser* browser) {
  // In kiosk mode, we want to always be fullscreen.
  if (IsKioskModeEnabled() || base::CommandLine::ForCurrentProcess()->HasSwitch(
                                  switches::kStartFullscreen)) {
    chrome::ToggleFullscreenMode(browser);
  }
}

void StartupBrowserCreatorImpl::Launch(
    Profile* profile,
    chrome::startup::IsProcessStartup process_startup,
    bool restore_tabbed_browser) {
  DCHECK(profile);
  profile_ = profile;

  DetermineURLsAndLaunch(process_startup, restore_tabbed_browser);

  if (command_line_->HasSwitch(switches::kInstallChromeApp)) {
    install_chrome_app::InstallChromeApp(
        command_line_->GetSwitchValueASCII(switches::kInstallChromeApp));
  }

  // It's possible for there to be no browser window, e.g. if someone
  // specified a non-sensical combination of options
  // ("--kiosk --no_startup_window"); do nothing in that case.
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (browser)
    MaybeToggleFullscreen(browser);
}

Browser* StartupBrowserCreatorImpl::OpenURLsInBrowser(
    Browser* browser,
    chrome::startup::IsProcessStartup process_startup,
    const std::vector<GURL>& urls) {
  StartupTabs tabs;
  UrlsToTabs(urls, &tabs);
  return OpenTabsInBrowser(browser, process_startup, tabs);
}

Browser* StartupBrowserCreatorImpl::OpenTabsInBrowser(
    Browser* browser,
    chrome::startup::IsProcessStartup process_startup,
    const StartupTabs& tabs) {
  DCHECK(!tabs.empty());

  // If we don't yet have a profile, try to use the one we're given from
  // |browser|. While we may not end up actually using |browser| (since it
  // could be a popup window), we can at least use the profile.
  if (!profile_ && browser)
    profile_ = browser->profile();

  if (!browser || !browser->is_type_normal()) {
    CHECK(profile_);
    // In some conditions a new browser object cannot be created. The most
    // common reason for not being able to create browser is having this call
    // when the browser process is shutting down. This can also fail if the
    // passed profile is of a type that is not suitable for browser creation.
    if (Browser::GetCreationStatusForProfile(profile_) !=
        Browser::CreationStatus::kOk) {
      return nullptr;
    }
    // Startup browsers are not counted as being created by a user_gesture
    // because of historical accident, even though the startup browser was
    // created in response to the user clicking on chrome. There was an
    // incomplete check on whether a user gesture created a window which looked
    // at the state of the MessageLoop.
    Browser::CreateParams params = Browser::CreateParams(profile_, false);
    params.creation_source = Browser::CreationSource::kStartupCreator;
#if BUILDFLAG(IS_LINUX)
    params.startup_id =
        command_line_->GetSwitchValueASCII("desktop-startup-id");
#endif
    if (command_line_->HasSwitch(switches::kWindowName)) {
      params.user_title =
          command_line_->GetSwitchValueASCII(switches::kWindowName);
    }

    browser = Browser::Create(params);
  }
  CHECK(profile_);

  bool first_tab = true;
  bool process_headless_commands = headless::ShouldProcessHeadlessCommands();
  custom_handlers::ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(profile_);
  for (auto& tab : tabs) {
    // We skip URLs that we'd have to launch an external protocol handler for.
    // This avoids us getting into an infinite loop asking ourselves to open
    // a URL, should the handler be (incorrectly) configured to be us. Anyone
    // asking us to open such a URL should really ask the handler directly.
    bool handled_by_chrome =
        ProfileIOData::IsHandledURL(tab.url) ||
        (registry && registry->IsHandledProtocol(tab.url.scheme()));
    if (process_startup == chrome::startup::IsProcessStartup::kNo &&
        !handled_by_chrome) {
      continue;
    }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    // Start the What's New fetch but don't add the tab at this point. The tab
    // will open as the foreground tab only if the remote content can be
    // retrieved successfully. This prevents needing to automatically close the
    // tab after opening it in the case where What's New does not load.
    if (tab.url == whats_new::GetWebUIStartupURL()) {
      whats_new::StartWhatsNewFetch(browser);
      continue;
    }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

    // Headless mode is restricted to only one url in the command line, so
    // just grab the first one assuming it's the target.
    if (first_tab && process_headless_commands) {
      std::unique_ptr<ScopedProfileKeepAlive> profile_keepalive;
      if (!profile_->IsOffTheRecord()) {
        profile_keepalive = std::make_unique<ScopedProfileKeepAlive>(
            profile_, ProfileKeepAliveOrigin::kHeadlessCommand);
      }
      headless::ProcessHeadlessCommands(
          profile_, tab.url,
          base::BindOnce(
              [](base::WeakPtr<Browser> browser,
                 std::unique_ptr<ScopedProfileKeepAlive> profile_keepalive,
                 headless::HeadlessCommandHandler::Result result) {
                if (browser && browser->window()) {
#if BUILDFLAG(IS_MAC)
                  // On Macs Chrome keeps running after the last browser
                  // window is closed which is not expected for headless
                  // command execution, so explicitly allow application
                  // to terminate after the browser window is closed.
                  app_controller_mac::AllowApplicationToTerminate();
#endif
                  browser->window()->Close();
                }
              },
              browser->AsWeakPtr(), std::move(profile_keepalive)));
      continue;
    }

    int add_types = first_tab ? AddTabTypes::ADD_ACTIVE : AddTabTypes::ADD_NONE;
    add_types |= AddTabTypes::ADD_FORCE_INDEX;
    if (tab.type == StartupTab::Type::kPinned)
      add_types |= AddTabTypes::ADD_PINNED;

    NavigateParams params(browser, tab.url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
    params.disposition = first_tab ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                                   : WindowOpenDisposition::NEW_BACKGROUND_TAB;
    params.tabstrip_add_types = add_types;

#if BUILDFLAG(ENABLE_RLZ)
    if (process_startup == chrome::startup::IsProcessStartup::kYes &&
        google_util::IsGoogleHomePageUrl(tab.url)) {
      params.extra_headers = rlz::RLZTracker::GetAccessPointHttpHeader(
          rlz::RLZTracker::ChromeHomePage());
    }
#endif  // BUILDFLAG(ENABLE_RLZ)

    Navigate(&params);
    first_tab = false;
  }
  if (!browser->tab_strip_model()->GetActiveWebContents() &&
      !process_headless_commands) {
    // TODO(sky): this is a work around for 110909. Figure out why it's needed.
    if (!browser->tab_strip_model()->count())
      chrome::AddTabAt(browser, GURL(), -1, true);
    else
      browser->tab_strip_model()->ActivateTabAt(0);
  }

  browser->window()->Show();

  return browser;
}

void StartupBrowserCreatorImpl::DetermineURLsAndLaunch(
    chrome::startup::IsProcessStartup process_startup,
    bool restore_tabbed_browser) {
  if (StartupBrowserCreator::ShouldLoadProfileWithoutWindow(*command_line_)) {
    // Checking the flags this late in the launch should be redundant.
    // TODO(crbug.com/40216113): Remove by M104.
    NOTREACHED_IN_MIGRATION();
    base::debug::DumpWithoutCrashing();
    return;
  }

  const bool is_incognito_or_guest = profile_->IsOffTheRecord();
  bool is_post_crash_launch = HasPendingUncleanExit(profile_);
  bool has_incompatible_applications = false;
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (is_post_crash_launch) {
    // Check if there are any incompatible applications cached from the last
    // Chrome run.
    has_incompatible_applications =
        IncompatibleApplicationsUpdater::HasCachedApplications();
  }
#endif

  // Presentation of promotional and/or educational tabs may be controlled via
  // administrative policy.
  bool promotions_enabled = true;
  const PrefService::Preference* promotions_enabled_pref = nullptr;
  PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    promotions_enabled_pref =
        local_state->FindPreference(prefs::kPromotionsEnabled);
  }
  if (promotions_enabled_pref && promotions_enabled_pref->IsManaged()) {
    // Presentation is managed; obey the policy setting.
    promotions_enabled = promotions_enabled_pref->GetValue()->GetBool();
  } else {
    // Presentation is not managed. Infer an intent to disable if any value for
    // the RestoreOnStartup policy is mandatory or recommended.
    promotions_enabled =
        !SessionStartupPref::TypeIsManaged(profile_->GetPrefs()) &&
        !SessionStartupPref::TypeHasRecommendedValue(profile_->GetPrefs());
  }

  const bool whats_new_enabled =
      whats_new::ShouldShowForState(local_state, promotions_enabled);

  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(profile_);

  bool privacy_sandbox_dialog_required = false;
  if (privacy_sandbox_service) {
    switch (privacy_sandbox_service->GetRequiredPromptType(
        PrivacySandboxService::SurfaceType::kDesktop)) {
      case PrivacySandboxService::PromptType::kM1Consent:
      case PrivacySandboxService::PromptType::kM1NoticeEEA:
      case PrivacySandboxService::PromptType::kM1NoticeROW:
      case PrivacySandboxService::PromptType::kM1NoticeRestricted:
        privacy_sandbox_dialog_required = true;
        break;
      case PrivacySandboxService::PromptType::kNone:
        break;
    }
  }

  auto result = DetermineStartupTabs(
      StartupTabProviderImpl(), process_startup, is_incognito_or_guest,
      is_post_crash_launch, has_incompatible_applications, promotions_enabled,
      whats_new_enabled, privacy_sandbox_dialog_required);
  StartupTabs tabs = std::move(result.tabs);

  // Return immediately if we start an async restore, since the remainder of
  // that process is self-contained.
  if (MaybeAsyncRestore(tabs, process_startup, is_post_crash_launch)) {
    return;
  }
  BrowserOpenBehaviorOptions behavior_options = 0;
  if (process_startup == chrome::startup::IsProcessStartup::kYes)
    behavior_options |= PROCESS_STARTUP;
  if (is_post_crash_launch)
    behavior_options |= IS_POST_CRASH_LAUNCH;
  if (command_line_->HasSwitch(switches::kOpenInNewWindow))
    behavior_options |= HAS_NEW_WINDOW_SWITCH;
  if (result.launch_result == LaunchResult::kWithGivenUrls)
    behavior_options |= HAS_CMD_LINE_TABS;

  BrowserOpenBehavior behavior = DetermineBrowserOpenBehavior(
      StartupBrowserCreator::GetSessionStartupPref(*command_line_, profile_),
      behavior_options);

  SessionRestore::BehaviorBitmask restore_options =
      restore_tabbed_browser ? SessionRestore::RESTORE_BROWSER : 0;
  if (behavior == BrowserOpenBehavior::SYNCHRONOUS_RESTORE) {
#if BUILDFLAG(IS_MAC)
    bool was_mac_login_or_resume = base::mac::WasLaunchedAsLoginOrResumeItem();
#else
    bool was_mac_login_or_resume = false;
#endif
    restore_options = DetermineSynchronousRestoreOptions(
        browser_defaults::kAlwaysCreateTabbedBrowserOnSessionRestore,
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kCreateBrowserOnStartupForTests),
        was_mac_login_or_resume, restore_tabbed_browser);
  }

  Browser* browser = RestoreOrCreateBrowser(
      tabs, behavior, restore_options, process_startup, is_post_crash_launch);

  // Finally, add info bars.
  AddInfoBarsIfNecessary(browser, profile_, *command_line_, is_first_run_,
                         /*is_web_app=*/false);
}

StartupBrowserCreatorImpl::DetermineStartupTabsResult::
    DetermineStartupTabsResult(StartupTabs tabs, LaunchResult launch_result)
    : tabs(std::move(tabs)), launch_result(launch_result) {}

StartupBrowserCreatorImpl::DetermineStartupTabsResult::
    DetermineStartupTabsResult(DetermineStartupTabsResult&&) = default;

StartupBrowserCreatorImpl::DetermineStartupTabsResult&
StartupBrowserCreatorImpl::DetermineStartupTabsResult::operator=(
    DetermineStartupTabsResult&&) = default;

StartupBrowserCreatorImpl::DetermineStartupTabsResult::
    ~DetermineStartupTabsResult() = default;

StartupBrowserCreatorImpl::DetermineStartupTabsResult
StartupBrowserCreatorImpl::DetermineStartupTabs(
    const StartupTabProvider& provider,
    chrome::startup::IsProcessStartup process_startup,
    bool is_incognito_or_guest,
    bool is_post_crash_launch,
    bool has_incompatible_applications,
    bool promotions_enabled,
    bool whats_new_enabled,
    bool privacy_sandbox_dialog_required) {
  StartupTabs tabs =
      provider.GetCommandLineTabs(*command_line_, cur_dir_, profile_);
  LaunchResult launch_result =
      tabs.empty() ? LaunchResult::kNormally : LaunchResult::kWithGivenUrls;

  if (whats_new_enabled && (launch_result == LaunchResult::kWithGivenUrls ||
                            is_incognito_or_guest || is_post_crash_launch)) {
    whats_new::LogStartupType(whats_new::StartupType::kIneligible);
  }

  // Only the New Tab Page or command line URLs may be shown in incognito mode.
  // A similar policy exists for crash recovery launches, to prevent getting the
  // user stuck in a crash loop.
  if (is_incognito_or_guest || is_post_crash_launch) {
    if (!tabs.empty())
      return {std::move(tabs), launch_result};

    if (is_post_crash_launch) {
      tabs = provider.GetPostCrashTabs(has_incompatible_applications);
      if (!tabs.empty())
        return {std::move(tabs), launch_result};
    }

    return {StartupTabs({StartupTab(GURL(chrome::kChromeUINewTabURL))}),
            launch_result};
  }

  // A trigger on a profile may indicate that we should show a tab which
  // offers to reset the user's settings.  When this appears, it is first, and
  // may be shown alongside command-line tabs.
  StartupTabs reset_tabs = provider.GetResetTriggerTabs(profile_);

  // URLs passed on the command line supersede all others, except pinned tabs.
  PrependTabs(reset_tabs, &tabs);

  if (launch_result == LaunchResult::kNormally) {
    // An initial preferences file provided with this distribution may specify
    // tabs to be displayed on first run, overriding all non-command-line tabs,
    // including the profile reset tab.
    StartupTabs distribution_tabs =
        provider.GetDistributionFirstRunTabs(browser_creator_);
    if (!distribution_tabs.empty())
      return {std::move(distribution_tabs), launch_result};

    // Whether a first run experience was or will be shown as part of this
    // startup.
    bool has_first_run_experience = false;
    if (promotions_enabled) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      if (is_first_run_ == chrome::startup::IsFirstRun::kYes) {
        // We just showed the first run experience in the Desktop FRE window.
        has_first_run_experience = true;
      }
#endif

      // Potentially add the What's New Page. Note that the What's New page
      // should never be shown in the same session as any first-run onboarding
      // tabs. It also shouldn't be shown with reset tabs that are required to
      // always be the first foreground tab.
      if (!has_first_run_experience && reset_tabs.empty()) {
        StartupTabs new_features_tabs;
        new_features_tabs = provider.GetNewFeaturesTabs(whats_new_enabled);
        AppendTabs(new_features_tabs, &tabs);
      } else if (whats_new_enabled) {
        whats_new::LogStartupType(whats_new::StartupType::kOverridden);
      }
    }

    // If the user has set the preference indicating URLs to show on opening,
    // read and add those.
    StartupTabs prefs_tabs =
        provider.GetPreferencesTabs(*command_line_, profile_);
    AppendTabs(prefs_tabs, &tabs);

    // Potentially add the New Tab Page.
    // Note that URLs from preferences are explicitly meant to override showing
    // the NTP.
    if (prefs_tabs.empty()) {
      AppendTabs(provider.GetNewTabPageTabs(*command_line_, profile_), &tabs);
    }

    // Potentially add a tab appropriate to display the Privacy Sandbox
    // confirmaton dialog on top of. Ideally such a tab will already exist
    // in |tabs|, and no additional tab will be required.
    if (privacy_sandbox_dialog_required &&
        launch_result == LaunchResult::kNormally) {
      AppendTabs(provider.GetPrivacySandboxTabs(profile_, tabs), &tabs);
    }
  }

  // Maybe add any tabs which the user has previously pinned.
  AppendTabs(provider.GetPinnedTabs(*command_line_, profile_), &tabs);

  return {std::move(tabs), launch_result};
}

bool StartupBrowserCreatorImpl::MaybeAsyncRestore(
    const StartupTabs& tabs,
    chrome::startup::IsProcessStartup process_startup,
    bool is_post_crash_launch) {
  // Restore is performed synchronously on startup, and is never performed when
  // launching after crashing.
  if (process_startup == chrome::startup::IsProcessStartup::kYes ||
      is_post_crash_launch) {
    return false;
  }

  // Note: there's no session service in incognito or guest mode.
  if (!SessionServiceFactory::GetForProfileForSessionRestore(profile_))
    return false;

  bool restore_apps =
      ShouldRestoreApps(StartupBrowserCreator::WasRestarted(), profile_);
  // Note: there's no session service in incognito or guest mode.
  SessionService* service =
      SessionServiceFactory::GetForProfileForSessionRestore(profile_);

  return service && service->RestoreIfNecessary(tabs, restore_apps);
}

Browser* StartupBrowserCreatorImpl::RestoreOrCreateBrowser(
    const StartupTabs& tabs,
    BrowserOpenBehavior behavior,
    SessionRestore::BehaviorBitmask restore_options,
    chrome::startup::IsProcessStartup process_startup,
    bool is_post_crash_launch) {
  Browser* browser = nullptr;
  if (behavior == BrowserOpenBehavior::SYNCHRONOUS_RESTORE) {
    // It's worth noting that this codepath is not hit by crash restore
    // because we want to avoid a crash restore loop, so we don't
    // automatically restore after a crash.
    // Crash restores are triggered via session_crashed_bubble_view.cc
    if (ShouldRestoreApps(StartupBrowserCreator::WasRestarted(), profile_))
      restore_options |= SessionRestore::RESTORE_APPS;

    browser = SessionRestore::RestoreSession(profile_, nullptr, restore_options,
                                             tabs);
    if (browser)
      return browser;
  } else if (behavior == BrowserOpenBehavior::USE_EXISTING) {
    browser = chrome::FindTabbedBrowser(
        profile_, process_startup == chrome::startup::IsProcessStartup::kYes);
  }

  base::AutoReset<bool> synchronous_launch_resetter(
      &StartupBrowserCreator::in_synchronous_profile_launch_, true);

  // OpenTabsInBrowser requires at least one tab be passed. As a fallback to
  // prevent a crash, use the NTP if |tabs| is empty. This could happen if
  // we expected a session restore to happen but it did not occur/succeed.
  browser = OpenTabsInBrowser(
      browser, process_startup,
      (tabs.empty()
           ? StartupTabs({StartupTab(GURL(chrome::kChromeUINewTabURL))})
           : tabs));

  // Now that a restore is no longer possible, it is safe to clear DOM storage,
  // unless this is a crash recovery.
  if (!is_post_crash_launch) {
    profile_->GetDefaultStoragePartition()
        ->GetDOMStorageContext()
        ->StartScavengingUnusedSessionStorage();
  }

  return browser;
}

// static
StartupBrowserCreatorImpl::BrowserOpenBehavior
StartupBrowserCreatorImpl::DetermineBrowserOpenBehavior(
    const SessionStartupPref& pref,
    BrowserOpenBehaviorOptions options) {
  if (!(options & PROCESS_STARTUP)) {
    // For existing processes, restore would have happened before invoking this
    // function. If Chrome was launched with passed URLs, assume these should
    // be appended to an existing window if possible, unless overridden by a
    // switch.
    return ((options & HAS_CMD_LINE_TABS) && !(options & HAS_NEW_WINDOW_SWITCH))
               ? BrowserOpenBehavior::USE_EXISTING
               : BrowserOpenBehavior::NEW;
  }

  if (pref.ShouldRestoreLastSession()) {
    // Don't perform a session restore on a post-crash launch, as this could
    // cause a crash loop.
    if (!(options & IS_POST_CRASH_LAUNCH))
      return BrowserOpenBehavior::SYNCHRONOUS_RESTORE;
  }

  return BrowserOpenBehavior::NEW;
}

// static
SessionRestore::BehaviorBitmask
StartupBrowserCreatorImpl::DetermineSynchronousRestoreOptions(
    bool has_create_browser_default,
    bool has_create_browser_switch,
    bool was_mac_login_or_resume,
    bool restore_tabbed_browser) {
  SessionRestore::BehaviorBitmask options = SessionRestore::SYNCHRONOUS;

  if (restore_tabbed_browser) {
    options |= SessionRestore::RESTORE_BROWSER;
  }

  // Suppress the creation of a new window on Mac when restoring with no windows
  // if launching Chrome via a login item or the resume feature in OS 10.7+.
  if (!was_mac_login_or_resume &&
      (has_create_browser_default || has_create_browser_switch))
    options |= SessionRestore::ALWAYS_CREATE_TABBED_BROWSER;

  return options;
}

// static
bool StartupBrowserCreatorImpl::IsKioskModeEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kKioskMode);
}
