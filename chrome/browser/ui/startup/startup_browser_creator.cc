// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include <stddef.h>

#include <optional>
#include <set>
#include <string>
#include <utility>

#include "apps/switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/scoped_multi_source_observation.h"
#include "base/strings/string_tokenizer.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/trace_event/trace_event.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "chrome/browser/apps/platform_apps/platform_app_launch.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/extensions/startup_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/exit_type_service.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/startup/startup_tab_provider.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/startup/web_app_startup_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/util.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/switches.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/extensions/api/messaging/native_messaging_launch_from_native.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/startup/first_run_service.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge_win.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/ui/startup/web_app_info_recorder_utils.h"
#include "components/headless/policy/headless_mode_policy.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"
#endif

using content::BrowserThread;
using content::ChildProcessSecurityPolicy;

namespace {

// Keeps track on which profiles have been launched.
class ProfileLaunchObserver : public ProfileObserver,
                              public BrowserListObserver {
 public:
  ProfileLaunchObserver() { BrowserList::AddObserver(this); }
  ProfileLaunchObserver(const ProfileLaunchObserver&) = delete;
  ProfileLaunchObserver& operator=(const ProfileLaunchObserver&) = delete;
  ~ProfileLaunchObserver() override { BrowserList::RemoveObserver(this); }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    opened_profiles_.insert(browser->profile());
    MaybeActivateProfile();
  }

  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    observed_profiles_.RemoveObservation(profile);
    launched_profiles_.erase(profile);
    opened_profiles_.erase(profile);
    if (profile == profile_to_activate_)
      profile_to_activate_ = nullptr;
    // If this profile was the last launched one without an opened window,
    // then we may be ready to activate |profile_to_activate_|.
    MaybeActivateProfile();
  }

  // Returns true if `profile` has been launched by
  // StartupBrowserCreator::LaunchBrowser() and has at least one open window.
  bool HasBeenLaunchedAndBrowserOpen(const Profile* profile) const {
    return base::Contains(opened_profiles_, profile) &&
           base::Contains(launched_profiles_, profile);
  }

  void AddLaunched(Profile* profile) {
    if (!observed_profiles_.IsObservingSource(profile))
      observed_profiles_.AddObservation(profile);
    launched_profiles_.insert(profile);
    if (chrome::FindBrowserWithProfile(profile)) {
      // A browser may get opened before we get initialized (e.g., in tests),
      // so we never see the OnBrowserAdded() for it.
      opened_profiles_.insert(profile);
    }
  }

  void Clear() {
    launched_profiles_.clear();
    opened_profiles_.clear();
  }

  bool activated_profile() { return activated_profile_; }

  void set_profile_to_activate(Profile* profile) {
    if (!observed_profiles_.IsObservingSource(profile))
      observed_profiles_.AddObservation(profile);
    profile_to_activate_ = profile;
    MaybeActivateProfile();
  }

 private:
  void MaybeActivateProfile() {
    if (!profile_to_activate_)
      return;
    // Check that browsers have been opened for all the launched profiles.
    // Note that browsers opened for profiles that were not added as launched
    // profiles are simply ignored.
    auto i = launched_profiles_.begin();
    for (; i != launched_profiles_.end(); ++i) {
      if (opened_profiles_.find(*i) == opened_profiles_.end())
        return;
    }
    // Asynchronous post to give a chance to the last window to completely
    // open and activate before trying to activate |profile_to_activate_|.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ProfileLaunchObserver::ActivateProfile,
                                  base::Unretained(this)));
    // Avoid posting more than once before ActivateProfile gets called.
    observed_profiles_.RemoveAllObservations();
    BrowserList::RemoveObserver(this);
  }

  void ActivateProfile() {
    // We need to test again, in case the profile got deleted in the mean time.
    if (profile_to_activate_) {
      Browser* browser = chrome::FindBrowserWithProfile(profile_to_activate_);
      // |profile| may never get launched, e.g., if it only had
      // incognito Windows and one of them was used to exit Chrome.
      // So it won't have a browser in that case.
      if (browser)
        browser->window()->Activate();
      // No need try to activate this profile again.
      profile_to_activate_ = nullptr;
    }
    // Assign true here, even if no browser was actually activated, so that
    // the test can stop waiting, and fail gracefully when needed.
    activated_profile_ = true;
  }

  // These are the profiles that get launched by
  // StartupBrowserCreator::LaunchBrowser.
  std::set<raw_ptr<const Profile, SetExperimental>> launched_profiles_;
  // These are the profiles for which at least one browser window has been
  // opened. This is needed to know when it is safe to activate
  // |profile_to_activate_|, otherwise, new browser windows being opened will
  // be activated on top of it.
  std::set<raw_ptr<const Profile, SetExperimental>> opened_profiles_;
  // This is null until the profile to activate has been chosen. This value
  // should only be set once all profiles have been launched, otherwise,
  // activation may not happen after the launch of newer profiles.
  raw_ptr<Profile, DanglingUntriaged> profile_to_activate_ = nullptr;
  // Set once we attempted to activate a profile. We only get one shot at this.
  bool activated_profile_ = false;
  base::ScopedMultiSourceObservation<Profile, ProfileObserver>
      observed_profiles_{this};
};

base::LazyInstance<ProfileLaunchObserver>::DestructorAtExit
    profile_launch_observer = LAZY_INSTANCE_INITIALIZER;

// Dumps the current set of the browser process's histograms to |output_file|.
// The file is overwritten if it exists. This function should only be called in
// the blocking pool.
void DumpBrowserHistograms(const base::FilePath& output_file) {
  std::string output_string(
      base::StatisticsRecorder::ToJSON(base::JSON_VERBOSITY_LEVEL_FULL));

  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::WriteFile(output_file, output_string);
}

// Returns whether |profile_info.profile| can be opened during Chrome startup
// without explicit user action.
bool CanOpenProfileOnStartup(StartupProfileInfo profile_info) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, the user has already chosen and logged into the profile before
  // Chrome starts up.
  DCHECK_NE(profile_info.mode, StartupProfileMode::kProfilePicker);
  return true;
#else
  // Profile picker startups require explicit user action, profile can't be
  // readily opened.
  if (profile_info.mode == StartupProfileMode::kProfilePicker)
    return false;

  Profile* profile = profile_info.profile;

  // System profiles are not available.
  DCHECK(!profile->IsSystemProfile());

  // Profiles that require signin are not available.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (entry && entry->IsSigninRequired()) {
    return false;
  }

  if (profile->IsGuestSession()) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    return true;
#else
    // Guest is not available unless a there is already a guest browser open
    // (for example, launching a new browser after clicking on a downloaded file
    // in Guest mode).
    return chrome::GetBrowserCount(
               profile->GetPrimaryOTRProfile(/*create_if_needed=*/false)) > 0;
#endif
  }

  return true;
#endif
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
StartupProfileModeReason ShouldShowProfilePickerAtProcessLaunch(
    ProfileManager* profile_manager,
    bool has_command_line_specified_profile_directory,
    const base::CommandLine& command_line) {
  // Skip the profile picker when Chrome is restarted (e.g. after an update) so
  // that the session can be restored.
  if (StartupBrowserCreator::WasRestarted())
    return StartupProfileModeReason::kWasRestarted;

  // Don't show the picker if a certain profile (or an incognito window in the
  // default profile) is explicitly requested.
  if (profiles::IsGuestModeRequested(command_line,
                                     g_browser_process->local_state(),
                                     /*show_warning=*/false) ||
      command_line.HasSwitch(switches::kIncognito) ||
      has_command_line_specified_profile_directory) {
    // TODO(crbug.com/40257919): The profile directory and guest mode
    // were already tested in the calling function `GetStartupProfilePath()`.
    // Consolidate these checks.
    return StartupProfileModeReason::kIncognitoModeRequested;
  }

  // Don't show the picker if an app is explicitly requested to open. This URL
  // param should be ideally paired with switches::kProfileDirectory but it's
  // better to err on the side of opening the last profile than to err on the
  // side of not opening the app directly.
  if (command_line.HasSwitch(switches::kApp) ||
      command_line.HasSwitch(switches::kAppId)) {
    return StartupProfileModeReason::kAppRequested;
  }

#if BUILDFLAG(IS_WIN)
  // Don't show the picker if trying to uninstall an app. This URL param should
  // be paired with switches::kProfileDirectory but it's better to err on the
  // side of opening the last profile (and maybe fail uninstalling the app
  // there) than to err on the side of unexpectedly showing the picker UI.
  if (command_line.HasSwitch(switches::kUninstallAppId))
    return StartupProfileModeReason::kUninstallApp;

  // Don't show the picker if we want to perform a GCPW Sign In. It will want to
  // only launch an incognito window.
  if (command_line.HasSwitch(credential_provider::kGcpwSigninSwitch))
    return StartupProfileModeReason::kGcpwSignin;

  // If the browser is launched due to activation on Windows native
  // notification, the profile id encoded in the notification launch id should
  // be chosen over the profile picker.
  base::FilePath profile_basename =
      NotificationLaunchId::GetNotificationLaunchProfileBaseName(command_line);
  if (!profile_basename.empty()) {
    // TODO(crbug.com/40257919): The notification ID was already tested
    // in the calling function `GetStartupProfilePath()`. Consolidate these
    // checks.
    return StartupProfileModeReason::kNotificationLaunchIdWin2;
  }
#endif  // BUILDFLAG(IS_WIN)

  // Don't show the picker if Chrome should be launched without window. This
  // will also cause a profile to be loaded which Chrome needs for performing
  // background activity.
  if (StartupBrowserCreator::ShouldLoadProfileWithoutWindow(command_line))
    return StartupProfileModeReason::kLaunchWithoutWindow;

  return ProfilePicker::GetStartupModeReason();
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// If Incognito or Guest mode are requested by policy or command line returns
// the appropriate private browsing profile. Otherwise returns
// `profile_info.profile`.
Profile* GetPrivateProfileIfRequested(const base::CommandLine& command_line,
                                      StartupProfileInfo profile_info) {
  bool open_guest_profile = profiles::IsGuestModeRequested(
      command_line, g_browser_process->local_state(),
      /* show_warning= */ true);
  if (open_guest_profile) {
    Profile* profile = g_browser_process->profile_manager()->GetProfile(
        ProfileManager::GetGuestProfilePath());
    profile = profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    return profile;
  }

  if (profile_info.mode == StartupProfileMode::kProfilePicker) {
    // Profile not intended for direct usage, we can exit early.
    return profile_info.profile;
  }

  Profile* profile = profile_info.profile;
  if (IncognitoModePrefs::ShouldLaunchIncognito(command_line,
                                                profile->GetPrefs())) {
    return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  } else {
    bool expect_incognito = command_line.HasSwitch(switches::kIncognito);
    LOG_IF(WARNING, expect_incognito)
        << "Incognito mode disabled by policy, launching a normal "
        << "browser session.";
  }

  return profile;
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
StartupProfileInfo GetProfilePickerStartupProfileInfo() {
  // We can only show the profile picker if the system profile (where the
  // profile picker lives) also exists (or is creatable).
  // TODO(crbug.com/40205861): Remove unnecessary system profile check here.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager->GetProfile(ProfileManager::GetSystemProfilePath()))
    return {.profile = nullptr, .mode = StartupProfileMode::kError};

  return {.profile = nullptr, .mode = StartupProfileMode::kProfilePicker};
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

bool IsSilentLaunchEnabled(const base::CommandLine& command_line,
                           const Profile* profile) {
  // This check should have been done in `ProcessCmdLineImpl()` before calling
  // this function. But `LaunchBrowser()` can be called directly, for example by
  // //chrome/browser/ash/login/session/user_session_manager.cc, so it has to be
  // checked again.
  // TODO(crbug.com/40819749): Investigate minimizing duplicate checks.

  if (command_line.HasSwitch(switches::kNoStartupWindow))
    return true;

  if (command_line.HasSwitch(switches::kSilentLaunch))
    return true;

  if (StartupBrowserCreator::ShouldLoadProfileWithoutWindow(command_line))
    return true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return profile->GetPrefs()->GetBoolean(
      prefs::kStartupBrowserWindowLaunchSuppressed);
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

bool CanOpenWebApp(Profile* profile) {
  return web_app::AreWebAppsEnabled(profile) &&
         apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile);
}

// Handles the --app switch.
bool MaybeLaunchAppShortcutWindow(const base::CommandLine& command_line,
                                  const base::FilePath& cur_dir,
                                  chrome::startup::IsFirstRun is_first_run,
                                  Profile* profile) {
  if (!profile) {
    return false;
  }

  if (!command_line.HasSwitch(switches::kApp))
    return false;

  std::string url_string = command_line.GetSwitchValueASCII(switches::kApp);
  if (url_string.empty())
    return false;

#if BUILDFLAG(IS_WIN)  // Fix up Windows shortcuts.
  base::ReplaceSubstringsAfterOffset(&url_string, 0, "\\x", "%");
#endif
  GURL url(url_string);

  // Restrict allowed URLs for --app switch.
  if (!url.is_empty() && url.is_valid()) {
    content::ChildProcessSecurityPolicy* policy =
        content::ChildProcessSecurityPolicy::GetInstance();
    if (policy->IsWebSafeScheme(url.scheme()) ||
        url.SchemeIs(url::kFileScheme)) {
      const content::WebContents* web_contents =
          apps::OpenExtensionAppShortcutWindow(profile, url);
      if (web_contents) {
        web_app::startup::FinalizeWebAppLaunch(
            web_app::startup::OpenMode::kInWindowByUrl, command_line,
            is_first_run, chrome::FindBrowserWithTab(web_contents),
            apps::LaunchContainer::kLaunchContainerWindow);
        return true;
      }
    }
  }
  return false;
}

bool MaybeLaunchExtensionApp(const base::CommandLine& command_line,
                             const base::FilePath& cur_dir,
                             chrome::startup::IsFirstRun is_first_run,
                             Profile* profile) {
  if (!command_line.HasSwitch(switches::kAppId))
    return false;

  std::string app_id = command_line.GetSwitchValueASCII(switches::kAppId);
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->GetInstalledExtension(
          app_id);
  if (!extension)
    return false;

  LaunchAppWithCallback(
      profile, app_id, command_line, cur_dir,
      base::BindOnce(&web_app::startup::FinalizeWebAppLaunch,
                     web_app::startup::OpenMode::kInWindowByAppId, command_line,
                     is_first_run));
  return true;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Updates need to be reflected in
// enum IncognitoForcedStart in tools/metrics/histograms/enums.xml.
enum class IncognitoForcedStart {
  kNoSwitchAndNotForced = 0,
  kSwitchButNotForced = 1,
  kNoSwitchButForced = 2,
  kSwitchAndForced = 3,
  kMaxValue = kSwitchAndForced,
};

void RecordIncognitoForcedStart(bool should_launch_incognito,
                                bool has_incognito_switch) {
  if (has_incognito_switch) {
    base::UmaHistogramEnumeration(
        "Startup.IncognitoForcedStart",
        should_launch_incognito ? IncognitoForcedStart::kSwitchAndForced
                                : IncognitoForcedStart::kSwitchButNotForced);
  } else {
    base::UmaHistogramEnumeration(
        "Startup.IncognitoForcedStart",
        should_launch_incognito ? IncognitoForcedStart::kNoSwitchButForced
                                : IncognitoForcedStart::kNoSwitchAndNotForced);
  }
}

#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(ENABLE_DICE_SUPPORT)
// Launches a browser by using a dedicated `StartupBrowserCreator`, to avoid
// having to rely on the current instance staying alive while this method is
// bound as a callback.
void OpenNewWindowForFirstRun(
    const base::CommandLine& command_line,
    Profile* profile,
    const base::FilePath& cur_dir,
    const std::vector<GURL>& first_run_urls,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    bool proceed) {
  if (!proceed)
    return;

  StartupBrowserCreator browser_creator;
  browser_creator.AddFirstRunTabs(first_run_urls);
  browser_creator.LaunchBrowser(command_line, profile, cur_dir, process_startup,
                                is_first_run, /*restore_tabbed_browser=*/true);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns the app id of the kiosk app associated with the current user session.
// Returns nullopt for non-kiosk user sessions, since crash recovery is not
// supported there.
std::optional<ash::KioskAppId> GetAppId(const base::CommandLine& command_line,
                                        Profile* profile) {
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);

  if (!user) {
    return std::nullopt;
  }

  switch (user->GetType()) {
    case user_manager::UserType::kKioskApp:
      return ash::KioskAppId::ForChromeApp(
          command_line.GetSwitchValueASCII(::switches::kAppId),
          user->GetAccountId());
    case user_manager::UserType::kWebKioskApp:
      return ash::KioskAppId::ForWebApp(user->GetAccountId());
    case user_manager::UserType::kKioskIWA:
      return ash::KioskAppId::ForIsolatedWebApp(user->GetAccountId());
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
    case user_manager::UserType::kGuest:
    case user_manager::UserType::kPublicAccount:
      return std::nullopt;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

StartupProfileMode StartupProfileModeFromReason(
    StartupProfileModeReason reason) {
  switch (reason) {
    case StartupProfileModeReason::kError:
      return StartupProfileMode::kError;

    case StartupProfileModeReason::kMultipleProfiles:
    case StartupProfileModeReason::kPickerForcedByPolicy:
      return StartupProfileMode::kProfilePicker;

    case StartupProfileModeReason::kGuestModeRequested:
    case StartupProfileModeReason::kGuestSessionLacros:
    case StartupProfileModeReason::kProfileDirSwitch:
    case StartupProfileModeReason::kProfileEmailSwitch:
    case StartupProfileModeReason::kIgnoreProfilePicker:
    case StartupProfileModeReason::kCommandLineTabs:
    case StartupProfileModeReason::kPickerNotSupported:
    case StartupProfileModeReason::kWasRestarted:
    case StartupProfileModeReason::kIncognitoModeRequested:
    case StartupProfileModeReason::kAppRequested:
    case StartupProfileModeReason::kUninstallApp:
    case StartupProfileModeReason::kGcpwSignin:
    case StartupProfileModeReason::kLaunchWithoutWindow:
    case StartupProfileModeReason::kNotificationLaunchIdWin1:
    case StartupProfileModeReason::kNotificationLaunchIdWin2:
    case StartupProfileModeReason::kPickerDisabledByPolicy:
    case StartupProfileModeReason::kProfilesDisabledLacros:
    case StartupProfileModeReason::kSingleProfile:
    case StartupProfileModeReason::kInactiveProfiles:
    case StartupProfileModeReason::kUserOptedOut:
      return StartupProfileMode::kBrowserWindow;
  }
}

StartupBrowserCreator::StartupBrowserCreator() = default;

StartupBrowserCreator::~StartupBrowserCreator() {
  // When StartupBrowserCreator finishes doing its job, we should reset
  // was_restarted_read_ so that this browser no longer reads as restarted.
  // In the case where the browser is still running due to PWA or
  // background-extension subsequent startups should not execute restarted
  // behaviors.
  was_restarted_read_ = false;
}

// static
bool StartupBrowserCreator::was_restarted_read_ = false;

// static
bool StartupBrowserCreator::in_synchronous_profile_launch_ = false;

void StartupBrowserCreator::AddFirstRunTabs(const std::vector<GURL>& urls) {
  for (const auto& url : urls) {
    if (url.is_valid())
      first_run_tabs_.push_back(url);
  }
}

bool StartupBrowserCreator::Start(const base::CommandLine& cmd_line,
                                  const base::FilePath& cur_dir,
                                  StartupProfileInfo profile_info,
                                  const Profiles& last_opened_profiles) {
  TRACE_EVENT0("startup", "StartupBrowserCreator::Start");
  return ProcessCmdLineImpl(cmd_line, cur_dir,
                            chrome::startup::IsProcessStartup::kYes,
                            profile_info, last_opened_profiles);
}

// static
bool StartupBrowserCreator::InSynchronousProfileLaunch() {
  return in_synchronous_profile_launch_;
}

void StartupBrowserCreator::LaunchBrowser(
    const base::CommandLine& command_line,
    Profile* profile,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    bool restore_tabbed_browser) {
  TRACE_EVENT0("ui", "StartupBrowserCreator::LaunchBrowser");
  SCOPED_UMA_HISTOGRAM_TIMER("Startup.StartupBrowserCreator.LaunchBrowser");

  DCHECK(profile);
#if BUILDFLAG(IS_WIN)
  DCHECK(!command_line.HasSwitch(credential_provider::kGcpwSigninSwitch));
  DCHECK(!command_line.HasSwitch(switches::kNotificationLaunchId));
#endif  // BUILDFLAG(IS_WIN)
  in_synchronous_profile_launch_ =
      process_startup == chrome::startup::IsProcessStartup::kYes;

  profile = GetPrivateProfileIfRequested(
      command_line, {profile, StartupProfileMode::kBrowserWindow});

  if (!IsSilentLaunchEnabled(command_line, profile)) {
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(ENABLE_DICE_SUPPORT)
    auto* fre_service = FirstRunServiceFactory::GetForBrowserContext(profile);
    if (fre_service && fre_service->ShouldOpenFirstRun()) {
      // Show the FRE and let `OpenNewWindowForFirstRun()` handle the browser
      // launch. This `StartupBrowserCreator` will get destroyed when the method
      // returns so the relevant data is copied over and passed to the callback.
      fre_service->OpenFirstRunIfNeeded(
          FirstRunService::EntryPoint::kProcessStartup,
          base::BindOnce(&OpenNewWindowForFirstRun, command_line, profile,
                         cur_dir, first_run_tabs_, process_startup,
                         is_first_run));
      return;
    }
#endif

    StartupBrowserCreatorImpl lwp(cur_dir, command_line, this, is_first_run);
    lwp.Launch(profile,
               in_synchronous_profile_launch_
                   ? chrome::startup::IsProcessStartup::kYes
                   : chrome::startup::IsProcessStartup::kNo,
               restore_tabbed_browser);
  }
  in_synchronous_profile_launch_ = false;
  profile_launch_observer.Get().AddLaunched(profile);
}

void StartupBrowserCreator::LaunchBrowserForLastProfiles(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    StartupProfileInfo profile_info,
    const Profiles& last_opened_profiles,
    bool restore_tabbed_browser) {
  TRACE_EVENT0("ui", "StartupBrowserCreator::LaunchBrowserForLastProfiles");
  DCHECK_NE(profile_info.mode, StartupProfileMode::kError);

  Profile* profile = profile_info.profile;
  // On Windows, when chrome is launched by notification activation where the
  // kNotificationLaunchId switch is used, always use `profile` which contains
  // the profile id extracted from the notification launch id.
  bool was_windows_notification_launch = false;
#if BUILDFLAG(IS_WIN)
  was_windows_notification_launch =
      command_line.HasSwitch(switches::kNotificationLaunchId);
#endif  // BUILDFLAG(IS_WIN)

  if (profile_info.mode == StartupProfileMode::kProfilePicker) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    NOTREACHED_IN_MIGRATION();
#else
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        process_startup == chrome::startup::IsProcessStartup::kYes
            ? ProfilePicker::EntryPoint::kOnStartup
            : ProfilePicker::EntryPoint::kNewSessionOnExistingProcess));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    return;
  }

  // `last_opened_profiles` will be empty in the following circumstances:
  // - This is the first launch. `profile` is the initial profile.
  // - The user exited the browser by closing all windows for all profiles.
  //   `profile` is the profile which owned the last open window.
  // - Only incognito windows were open when the browser exited.
  //   `profile` is the last used incognito profile. Restoring it will create a
  //   browser window for the corresponding original profile.
  // - All of the last opened profiles fail to initialize.
  if (last_opened_profiles.empty() || was_windows_notification_launch) {
    if (CanOpenProfileOnStartup(profile_info)) {
      Profile* profile_to_open = profile->IsGuestSession()
                                     ? profile->GetPrimaryOTRProfile(
                                           /*create_if_needed=*/true)
                                     : profile;
#if BUILDFLAG(IS_CHROMEOS_ASH)
      if (process_startup == chrome::startup::IsProcessStartup::kYes) {
        if (ash::floating_workspace_util::IsFloatingWorkspaceV2Enabled()) {
          ash::FloatingWorkspaceServiceFactory::GetForProfile(profile_to_open);
        }
        // If floating workspace is enabled and safe mode is off, floating
        // workspace will handle the app restore from user's workspace copy.
        // Otherwise if safe mode is on, floating workspace will only emit
        // notification and then delegate the actual work to full restore.
        if (ash::floating_workspace_util::ShouldHandleRestartRestore()) {
          return;
        }
        // If FullRestoreService is available for the profile (i.e. the full
        // restore feature is enabled and the profile is a regular user
        // profile), defer the browser launching to FullRestoreService code.
        auto* full_restore_service =
            ash::full_restore::FullRestoreServiceFactory::GetForProfile(
                profile_to_open);
        if (full_restore_service) {
          full_restore_service->LaunchBrowserWhenReady();
          return;
        }
      }
#endif
      LaunchBrowser(command_line, profile_to_open, cur_dir, process_startup,
                    is_first_run, restore_tabbed_browser);
      return;
    }

    // Show ProfilePicker if `profile` can't be auto opened.
#if BUILDFLAG(IS_CHROMEOS_ASH)
    NOTREACHED_IN_MIGRATION();
#else
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        process_startup == chrome::startup::IsProcessStartup::kYes
            ? ProfilePicker::EntryPoint::kOnStartupNoProfile
            : ProfilePicker::EntryPoint::
                  kNewSessionOnExistingProcessNoProfile));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    return;
  }
  ProcessLastOpenedProfiles(command_line, cur_dir, process_startup,
                            is_first_run, profile, last_opened_profiles);
}

// static
bool StartupBrowserCreator::WasRestarted() {
  // Stores the value of the preference kWasRestarted had when it was read.
  static bool was_restarted = false;

  if (!was_restarted_read_) {
    PrefService* pref_service = g_browser_process->local_state();
    was_restarted = pref_service->GetBoolean(prefs::kWasRestarted);
    pref_service->SetBoolean(prefs::kWasRestarted, false);
    was_restarted_read_ = true;
  }
  return was_restarted;
}

// static
SessionStartupPref StartupBrowserCreator::GetSessionStartupPref(
    const base::CommandLine& command_line,
    const Profile* profile) {
  DCHECK(profile);
  const PrefService* prefs = profile->GetPrefs();
  SessionStartupPref pref = SessionStartupPref::GetStartupPref(prefs);

  // IsChromeFirstRun() looks for a sentinel file to determine whether the user
  // is starting Chrome for the first time. On Chrome OS, the sentinel is stored
  // in a location shared by all users and the check is meaningless. Query the
  // UserManager instead to determine whether the user is new.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const bool is_first_run =
      user_manager::UserManager::Get()->IsCurrentUserNew();
  // On ChromeOS restarts force the user to login again. The expectation is that
  // after a login the user gets clean state. For this reason we ignore
  // StartupBrowserCreator::WasRestarted(). However
  // StartupBrowserCreator::WasRestarted has to be called in order to correctly
  // update pref values.
  const bool did_restart = false;
  StartupBrowserCreator::WasRestarted();
#else
  const bool is_first_run = first_run::IsChromeFirstRun();
  const bool did_restart = StartupBrowserCreator::WasRestarted();
#endif

  // The pref has an OS-dependent default value. For the first run only, this
  // default is overridden with SessionStartupPref::DEFAULT so that first run
  // behavior (sync promo, welcome page) is consistently invoked.
  // This applies only if the pref is still at its default and has not been
  // set by the user, managed prefs or policy.
  if (is_first_run && SessionStartupPref::TypeIsDefault(prefs))
    pref.type = SessionStartupPref::DEFAULT;

  // The switches::kRestoreLastSession command line switch is used to restore
  // sessions after a browser self restart (e.g. after a Chrome upgrade).
  // However, new profiles can be created from a browser process that has this
  // switch so do not set the session pref to SessionStartupPref::LAST for
  // those as there is nothing to restore.
  bool restore_last_session =
      command_line.HasSwitch(switches::kRestoreLastSession);
  if ((restore_last_session || did_restart) && !profile->IsNewProfile()) {
    pref.type = SessionStartupPref::LAST;
  }

  // A browser starting for a profile being unlocked should always restore.
  if (!profile->IsGuestSession()) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath());

    if (entry && entry->IsSigninRequired())
      pref.type = SessionStartupPref::LAST;
  }
  if ((pref.ShouldRestoreLastSession() && !pref.ShouldOpenUrls()) &&
      (profile->IsGuestSession() || profile->IsOffTheRecord())) {
    // We don't store session information when incognito. If the user has
    // chosen to restore last session and launched incognito, fallback to
    // default launch behavior.
    pref.type = SessionStartupPref::DEFAULT;
  }

  return pref;
}

// static
void StartupBrowserCreator::ClearLaunchedProfilesForTesting() {
  profile_launch_observer.Get().Clear();
}

// static
void StartupBrowserCreator::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kPromotionsEnabled, true);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(prefs::kCommandLineFlagSecurityWarningsEnabled,
                                true);
#endif
  registry->RegisterBooleanPref(prefs::kSuppressUnsupportedOSWarning, false);
  registry->RegisterBooleanPref(prefs::kWasRestarted, false);

#if BUILDFLAG(IS_WIN)
  registry->RegisterStringPref(prefs::kShortcutMigrationVersion, std::string());
#endif
}

// static
void StartupBrowserCreator::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Default to true so that existing users are not shown the Welcome page.
  // ProfileManager handles setting this to false for new profiles upon
  // creation.
  registry->RegisterBooleanPref(prefs::kHasSeenWelcomePage, true);
#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // This will be set for newly created profiles, and is used to indicate which
  // users went through onboarding with the current experiment group.
  registry->RegisterStringPref(prefs::kNaviOnboardGroup, "");
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

// static
bool StartupBrowserCreator::ShouldLoadProfileWithoutWindow(
    const base::CommandLine& command_line) {
  // Don't open any browser windows if the command line indicates "background
  // mode".
  if (command_line.HasSwitch(switches::kNoStartupWindow))
    return true;

  return false;
}

bool StartupBrowserCreator::ProcessCmdLineImpl(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup process_startup,
    StartupProfileInfo profile_info,
    const Profiles& last_opened_profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_NE(profile_info.mode, StartupProfileMode::kError);
  TRACE_EVENT0("startup", "StartupBrowserCreator::ProcessCmdLineImpl");
  ComputeAndRecordLaunchMode(command_line);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  if (headless::IsHeadlessMode() &&
      headless::HeadlessModePolicy::IsHeadlessModeDisabled(
          g_browser_process->local_state())) {
    LOG(ERROR) << "Headless mode is disallowed by the system admin.";
    return false;
  }
#endif

  if (process_startup == chrome::startup::IsProcessStartup::kYes &&
      command_line.HasSwitch(switches::kDisablePromptOnRepost)) {
    content::NavigationController::DisablePromptOnRepost();
  }

  chrome::startup::IsFirstRun is_first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IsFirstRun::kYes
                                    : chrome::startup::IsFirstRun::kNo;

  bool silent_launch = false;
  bool should_launch_incognito =
      // Note: kIncognito and some related flags disable profile picker startups
      // via `ShouldShowProfilePickerAtProcessLaunch()`, so we can use it as
      // a signal here.
      // TODO(http://crbug.com/1293024): Refactor command line processing logic
      // to validate the flag sets and reliably determine the startup mode.
      profile_info.mode != StartupProfileMode::kProfilePicker &&
      IncognitoModePrefs::ShouldLaunchIncognito(
          command_line, profile_info.profile->GetPrefs());
  bool can_use_profile =
      CanOpenProfileOnStartup(profile_info) && !should_launch_incognito;

  RecordIncognitoForcedStart(should_launch_incognito,
                             command_line.HasSwitch(switches::kIncognito));

  // `profile` is never off-the-record. If Incognito or Guest enforcement switch
  // or policy are provided, use the appropriate private browsing profile
  // instead.
  Profile* privacy_safe_profile =
      GetPrivateProfileIfRequested(command_line, profile_info);

  if (command_line.HasSwitch(switches::kValidateCrx)) {
    if (process_startup == chrome::startup::IsProcessStartup::kNo) {
      LOG(ERROR) << "chrome is already running; you must close all running "
                 << "instances before running with the --"
                 << switches::kValidateCrx << " flag";
      return false;
    }
    extensions::StartupHelper helper;
    std::string message;
    std::string error;
    if (helper.ValidateCrx(command_line, &error))
      message = std::string("ValidateCrx Success");
    else
      message = std::string("ValidateCrx Failure: ") + error;
    printf("%s\n", message.c_str());
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)

  // The browser will be launched after the user logs in.
  if (command_line.HasSwitch(ash::switches::kLoginManager))
    silent_launch = true;

  if (IsRunningInForcedAppMode()) {
    // If we are here, it means the Chrome browser crashed/restarted while in
    // Kiosk mode, since the 'force app mode' switch is only added to the
    // commandline while in a kiosk session.
    Profile* profile = profile_info.profile;

    // Skip browser launch since app mode launches its app window.
    silent_launch = true;

    if (auto app_id = GetAppId(command_line, profile); app_id.has_value()) {
      ash::KioskController::Get().StartSessionAfterCrash(app_id.value(),
                                                         profile);
    } else {
      // If we are here, the user is invalid.
      // We should terminate the session in such cases.
      chrome::AttemptUserExit();
      return false;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  if (process_startup == chrome::startup::IsProcessStartup::kNo &&
      command_line.HasSwitch(switches::kDumpBrowserHistograms)) {
    // Only handle --dump-browser-histograms from a rendezvous. In this case, do
    // not open a new browser window even if no output file was given.
    base::FilePath output_file(
        command_line.GetSwitchValuePath(switches::kDumpBrowserHistograms));
    if (!output_file.empty()) {
      base::ThreadPool::PostTask(
          FROM_HERE,
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          base::BindOnce(&DumpBrowserHistograms, output_file));
    }
    silent_launch = true;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // Writes open and installed web apps to the specified file without
  // launching a new browser window or tab.
  if (base::FeatureList::IsEnabled(features::kListWebAppsSwitch) &&
      command_line.HasSwitch(switches::kListApps)) {
    base::FilePath output_file(
        command_line.GetSwitchValuePath(switches::kListApps));
    if (!output_file.empty() && output_file.IsAbsolute()) {
      base::FilePath profile_base_name(
          command_line.GetSwitchValuePath(switches::kProfileBaseName));
      chrome::startup::WriteWebAppsToFile(output_file, profile_base_name);
    }
    return true;
  }
#endif  //  BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if !BUILDFLAG(IS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kOnConnectNative) &&
      command_line.HasSwitch(switches::kNativeMessagingConnectHost) &&
      command_line.HasSwitch(switches::kNativeMessagingConnectExtension)) {
    // These flags are expected to be set together with `--no-startup-window`
    // and `switches::kProfileDirectory` which suppress the profile picker.
    if (profile_info.mode == StartupProfileMode::kProfilePicker) {
      // TODO(http://crbug.com/1293024): Refactor command line processing logic
      // to validate the flag sets and reliably determine the startup mode.
      LOG(ERROR)
          << "Failed to launch a native message host: couldn't pick a profile";
      NOTREACHED_IN_MIGRATION();
      base::debug::DumpWithoutCrashing();
    } else {
      extensions::LaunchNativeMessageHostFromNativeApp(
          command_line.GetSwitchValueASCII(
              switches::kNativeMessagingConnectExtension),
          command_line.GetSwitchValueASCII(
              switches::kNativeMessagingConnectHost),
          command_line.GetSwitchValueASCII(switches::kNativeMessagingConnectId),
          privacy_safe_profile);

      // Chrome's lifetime, if the specified extension and native messaging host
      // are both valid and a connection is established, is prolonged by
      // BackgroundModeManager. If `process_startup` is true,
      // --no-startup-window must be set or a browser window must be created for
      // BackgroundModeManager to start background mode. Without this, nothing
      // will take the first keep-alive and the browser process will not
      // terminate. To avoid this situation, don't set `silent_launch` in
      // response to the native messaging connect switches; require the client
      // to pass --no-startup-window if suppressing the creation of a window is
      // desired.
    }
  }

  if (web_app::IsolatedWebAppInstallationManager::HasIwaInstallSwitch(
          command_line)) {
    if (profile_info.mode == StartupProfileMode::kProfilePicker) {
      auto* profile_manager = g_browser_process->profile_manager();
      LOG(ERROR) << "Command line switches to install IWAs are incompatible "
                    "with the Profile Picker. If you have multiple profiles, "
                    "consider using the --"
                 << switches::kProfileDirectory
                 << " switch to select a profile (it accepts the name of a "
                    "profile directory in "
                 << profile_manager->user_data_dir() << ", such as '"
                 << profile_manager->GetLastUsedProfileDir().BaseName()
                 << "').";
      return false;
    } else {
      web_app::IsolatedWebAppInstallationManager::
          MaybeInstallIwaFromCommandLine(command_line, *privacy_safe_profile);
    }
  }
#endif  //  !BUILDFLAG(IS_CHROMEOS)

  // If --no-startup-window is specified then do not open a new window.
  if (command_line.HasSwitch(switches::kNoStartupWindow)) {
    silent_launch = true;
  }

  // If we don't want to launch a new browser window or tab we are done here.
  if (silent_launch) {
    bool should_block_browser_startup_metrics =
        process_startup == chrome::startup::IsProcessStartup::kYes;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Login screen is an expected common case. Startup metrics should still be
    // recorded after the user logs in even though this is a `silent_launch`.
    should_block_browser_startup_metrics &=
        !command_line.HasSwitch(ash::switches::kLoginManager);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    if (should_block_browser_startup_metrics) {
      startup_metric_utils::GetBrowser().SetNonBrowserUIDisplayed();
    }
    return true;
  }

#if BUILDFLAG(IS_WIN)
  // If --uninstall-app-id is specified, remove the target web app.
  if (command_line.HasSwitch(switches::kUninstallAppId)) {
    // `switches::kUninstallAppId` is expected to be set together with a
    // specific profile dir, which suppresses the profile picker, see
    // `ShouldShowProfilePickerAtProcessLaunch()`.
    // TODO(http://crbug.com/1293024): Refactor command line processing logic to
    // validate the flag sets and reliably determine the startup mode.
    CHECK_EQ(profile_info.mode, StartupProfileMode::kBrowserWindow)
        << "Failed to uninstall app: couldn't pick a profile";
    std::string app_id =
        command_line.GetSwitchValueASCII(switches::kUninstallAppId);

    web_app::WebAppProvider::GetForWebApps(privacy_safe_profile)
        ->ui_manager()
        .AsImpl()
        ->UninstallWebAppFromStartupSwitch(app_id);

    // Return true to allow startup to continue and for the main event loop to
    // run. The process will shut down if no browser windows are open when the
    // uninstall completes thanks to UninstallWebAppFromStartupSwitch's
    // ScopedKeepAlive.
    return true;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (command_line.HasSwitch(extensions::switches::kLoadApps) &&
      can_use_profile) {
    if (!ProcessLoadApps(command_line, cur_dir, privacy_safe_profile))
      return false;

    // Return early here to avoid opening a browser window.
    // The exception is when there are no browser windows, since we don't want
    // chrome to shut down.
    // TODO(jackhou): Do this properly once keep-alive is handled by the
    // background page of apps. Tracked at http://crbug.com/175381
    if (chrome::GetBrowserCount(privacy_safe_profile) != 0)
      return true;
  }

  // Check for --load-and-launch-app.
  if (command_line.HasSwitch(apps::kLoadAndLaunchApp) && can_use_profile) {
    base::CommandLine::StringType path =
        command_line.GetSwitchValueNative(apps::kLoadAndLaunchApp);

    if (!apps::AppLoadService::Get(privacy_safe_profile)
             ->LoadAndLaunch(base::FilePath(path), command_line, cur_dir)) {
      return false;
    }

    // Return early here since we don't want to open a browser window.
    // The exception is when there are no browser windows, since we don't want
    // chrome to shut down.
    // TODO(jackhou): Do this properly once keep-alive is handled by the
    // background page of apps. Tracked at http://crbug.com/175381
    if (chrome::GetBrowserCount(privacy_safe_profile) != 0)
      return true;
  }

#if BUILDFLAG(IS_WIN)
  if (command_line.HasSwitch(switches::kWinJumplistAction)) {
    // `switches::kWinJumplistAction` is expected to be set together with a
    // URL to open and with a specific profile dir.
    if (profile_info.mode == StartupProfileMode::kBrowserWindow) {
      // Use a non-NULL pointer to indicate JumpList has been used. We re-use
      // chrome::kJumpListIconDirname as the key to the data.
      privacy_safe_profile->SetUserData(
          chrome::kJumpListIconDirname,
          base::WrapUnique(new base::SupportsUserData::Data()));
    } else {
      // TODO(http://crbug.com/1293024): Refactor command line processing logic
      // to validate the flag sets and reliably determine the startup mode.
      DUMP_WILL_BE_NOTREACHED()
          << "Failed start for jumplist action: couldn't pick a profile";
    }
  }

  // If the command line has the kNotificationLaunchId switch, then this
  // call is from notification_helper.exe to process toast activation.
  // Delegate to the notification system; do not open a browser window here.
  if (command_line.HasSwitch(switches::kNotificationLaunchId)) {
    if (NotificationPlatformBridgeWin::HandleActivation(command_line)) {
      return true;
    }
    return false;
  }

  // If being started for credential provider logon purpose, only show the
  // signin page.
  if (command_line.HasSwitch(credential_provider::kGcpwSigninSwitch)) {
    // Having access to an incognito profile for this action (as checked below)
    // requires starting with a regular user profile (non-guest) and suppresses
    // profile picker startups, see `ShouldShowProfilePickerAtProcessLaunch()`.
    // TODO(http://crbug.com/1293024): Refactor command line processing logic to
    // validate the flag sets and reliably determine the startup mode.
    CHECK_EQ(profile_info.mode, StartupProfileMode::kBrowserWindow)
        << "Failed start for GCPW signin: couldn't pick a profile";

    // Use incognito profile since this is a credential provider logon.
    Profile* incognito_profile =
        privacy_safe_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    DCHECK(incognito_profile->IsIncognitoProfile());
    // NOTE: All launch urls are ignored when running with --gcpw-signin since
    // this mode only loads Google's sign in page.

    // If GCPW signin dialog fails, returning false here will allow Chrome to
    // exit gracefully during the launch.
    if (!StartGCPWSignin(command_line, incognito_profile))
      return false;

    return true;
  }
#endif  // BUILDFLAG(IS_WIN)

  if (command_line.HasSwitch(switches::kAppId)) {
    // `switches::kAppId` presence suppresses the profile picker, see
    // `ShouldShowProfilePickerAtProcessLaunch()`.
    // TODO(http://crbug.com/1293024): Refactor command line processing logic to
    // validate the flag sets and reliably determine the startup mode.
    CHECK_EQ(profile_info.mode, StartupProfileMode::kBrowserWindow)
        << "Failed launch with app: couldn't pick a profile";
    std::string app_id = command_line.GetSwitchValueASCII(switches::kAppId);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    // If Chrome Apps are deprecated and |app_id| is a Chrome App, display the
    // deprecation UI instead of launching the app.
    if (apps::OpenDeprecatedApplicationPrompt(privacy_safe_profile, app_id))
      return true;
#endif
    // If |app_id| is a disabled or terminated platform app we handle it
    // specially here, otherwise it will be handled below.
    if (apps::OpenExtensionApplicationWithReenablePrompt(
            privacy_safe_profile, app_id, command_line, cur_dir)) {
      return true;
    }
  }

  // TODO(http://crbug.com/1293024): Refactor command line processing logic to
  // validate the flag sets and reliably determine the startup mode.
  // Try a shortcut app launch (--app is present).
  // When running in incognito or guest mode, there typically won't be an
  // AppServiceProxyFactory available. The --app command line switch does not
  // require the AppServiceProxyFactory. This also allows the --app parameter
  // to work in conjunction with the --incognito command line parameter.
  if (MaybeLaunchAppShortcutWindow(command_line, cur_dir, is_first_run,
                                   privacy_safe_profile)) {
    return true;
  }

  // Launch the browser if the profile is unable to open web apps.
  if (!CanOpenWebApp(privacy_safe_profile)) {
    LaunchBrowserForLastProfiles(
        command_line, cur_dir, process_startup, is_first_run, profile_info,
        last_opened_profiles, /*restore_tabbed_browser=*/true);
    return true;
  }

  // Try a platform app launch.
  if (MaybeLaunchExtensionApp(command_line, cur_dir, is_first_run,
                              privacy_safe_profile)) {
    return true;
  }

  // This path is mostly used for Window and Linux, but also session restore for
  // Lacros.
  // On Chrome OS, app launches are routed through the AppService and
  // WebAppPublisherHelper.
  //
  // On Mac, PWA launch is normally handled in
  // web_app_shim_manager_delegate_mac.cc, but if an app shim for whatever
  // reason fails to dlopen chrome we can still end up here. While in that case
  // the launch behavior here isn't quite the correct behavior for an app launch
  // on Mac OS, this behavior is better than nothing and should result in the
  // app shim getting regenerated to hopefully fix future app launches.
  // TODO(crbug.com/40191242): Some integration tests also rely on this
  // code. Ideally those would be fixed to test the normal app launch path on
  // Mac instead, and this code should be changed to make it harder to
  // accidentally write tests that don't test the normal app launch path.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Try a web app launch.
  if (web_app::startup::MaybeHandleWebAppLaunch(
          command_line, cur_dir, privacy_safe_profile, is_first_run))
    return true;
#endif

  LaunchBrowserForLastProfiles(command_line, cur_dir, process_startup,
                               is_first_run, profile_info, last_opened_profiles,
                               /*restore_tabbed_browser=*/true);
  return true;
}

void StartupBrowserCreator::ProcessLastOpenedProfiles(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    Profile* last_used_profile,
    const Profiles& last_opened_profiles) {
  base::CommandLine command_line_without_urls(command_line.GetProgram());
  for (auto& switch_pair : command_line.GetSwitches()) {
    command_line_without_urls.AppendSwitchNative(switch_pair.first,
                                                 switch_pair.second);
  }

  // Launch the profiles in the order they became active.
  for (Profile* profile : last_opened_profiles) {
    DCHECK(!profile->IsGuestSession());

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // Skip any locked profile.
    if (!CanOpenProfileOnStartup({profile, StartupProfileMode::kBrowserWindow}))
      continue;

    // Guest profiles should not be reopened on startup. This can happen if
    // the last used profile was a Guest, but other profiles were also open
    // when Chrome was closed. In this case, pick a different open profile
    // to be the active one, since the Guest profile is never added to the
    // list of open profiles.
    if (last_used_profile->IsGuestSession())
      last_used_profile = profile;
#endif

    // Don't launch additional profiles which would only open a new tab
    // page. When restarting after an update, all profiles will reopen last
    // open pages.
    SessionStartupPref startup_pref =
        GetSessionStartupPref(command_line, profile);
    if (profile != last_used_profile &&
        startup_pref.type == SessionStartupPref::DEFAULT &&
        !HasPendingUncleanExit(profile)) {
      continue;
    }
    // Only record a launch mode histogram for |last_used_profile|. Pass a
    // null launch_mode_recorder for other profiles.
    LaunchBrowser((profile == last_used_profile) ? command_line
                                                 : command_line_without_urls,
                  profile, cur_dir, process_startup, is_first_run,
                  /*restore_tabbed_browser=*/true);
    // We've launched at least one browser.
    process_startup = chrome::startup::IsProcessStartup::kNo;
  }

// Set the |last_used_profile| to activate if a browser is launched for at
// least one profile. Otherwise, show UserManager.
// Note that this must be done after all profiles have
// been launched so the observer knows about all profiles to wait before
// activation this one.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (process_startup == chrome::startup::IsProcessStartup::kYes) {
    ProfilePicker::Show(ProfilePicker::Params::FromEntryPoint(
        ProfilePicker::EntryPoint::kOnStartup));
  } else  // NOLINT
#endif
  {
    profile_launch_observer.Get().set_profile_to_activate(last_used_profile);
  }
}

// static
bool StartupBrowserCreator::ProcessLoadApps(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile) {
  base::CommandLine::StringType path_list =
      command_line.GetSwitchValueNative(extensions::switches::kLoadApps);

  base::StringTokenizerT<base::CommandLine::StringType,
                         base::CommandLine::StringType::const_iterator>
      tokenizer(path_list, FILE_PATH_LITERAL(","));

  if (!tokenizer.GetNext())
    return false;

  base::FilePath app_absolute_dir =
      base::MakeAbsoluteFilePath(base::FilePath(tokenizer.token_piece()));
  if (!apps::AppLoadService::Get(profile)->LoadAndLaunch(
          app_absolute_dir, command_line, cur_dir)) {
    return false;
  }

  while (tokenizer.GetNext()) {
    app_absolute_dir =
        base::MakeAbsoluteFilePath(base::FilePath(tokenizer.token_piece()));

    if (!apps::AppLoadService::Get(profile)->Load(app_absolute_dir)) {
      return false;
    }
  }

  return true;
}

// static
void StartupBrowserCreator::ProcessCommandLineWithProfile(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    StartupProfileMode mode,
    Profile* profile) {
  DCHECK_NE(mode, StartupProfileMode::kError);
  if (mode == StartupProfileMode::kBrowserWindow && !profile) {
    LOG(ERROR) << "Failed to load the profile.";
    return;
  }
  Profiles last_opened_profiles;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS multiple profiles doesn't apply.
  // If no browser windows are open, i.e. the browser is being kept alive in
  // background mode or for other processing, restore |last_opened_profiles|.
  if (chrome::GetTotalBrowserCount() == 0) {
    last_opened_profiles =
        g_browser_process->profile_manager()->GetLastOpenedProfiles();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  StartupBrowserCreator startup_browser_creator;
  startup_browser_creator.ProcessCmdLineImpl(
      command_line, cur_dir, chrome::startup::IsProcessStartup::kNo,
      {profile, mode}, last_opened_profiles);
}

// static
void StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    const StartupProfilePathInfo& profile_path_info) {
  if (profile_path_info.reason == StartupProfileModeReason::kError) {
    return;
  }

  Profile* profile = nullptr;
  StartupProfileMode mode =
      StartupProfileModeFromReason(profile_path_info.reason);
  bool need_profile = mode == StartupProfileMode::kBrowserWindow;
  if (need_profile) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    profile = profile_manager->GetProfileByPath(profile_path_info.path);
    // The profile isn't loaded yet and so needs to be loaded asynchronously.
    if (!profile) {
      profile_manager->CreateProfileAsync(
          profile_path_info.path, base::BindOnce(&ProcessCommandLineWithProfile,
                                                 command_line, cur_dir, mode));
      return;
    }
  }

  ProcessCommandLineWithProfile(command_line, cur_dir, mode, profile);
}

// static
void StartupBrowserCreator::OpenStartupPages(
    Browser* browser,
    chrome::startup::IsProcessStartup process_startup) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun is_first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IsFirstRun::kYes
                                    : chrome::startup::IsFirstRun::kNo;
  StartupBrowserCreatorImpl startup_browser_creator_impl(
      base::FilePath(), command_line, is_first_run);
  SessionStartupPref session_startup_pref =
      StartupBrowserCreator::GetSessionStartupPref(command_line,
                                                   browser->profile());
  startup_browser_creator_impl.OpenURLsInBrowser(browser, process_startup,
                                                 session_startup_pref.urls);
}

// static
bool StartupBrowserCreator::ActivatedProfile() {
  return profile_launch_observer.Get().activated_profile();
}

bool HasPendingUncleanExit(Profile* profile) {
  return ExitTypeService::GetLastSessionExitType(profile) ==
             ExitType::kCrashed &&
         !profile_launch_observer.Get().HasBeenLaunchedAndBrowserOpen(
             profile) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kHideCrashRestoreBubble);
}

void AddLaunchedProfile(Profile* profile) {
  profile_launch_observer.Get().AddLaunched(profile);
}

StartupProfilePathInfo GetStartupProfilePath(
    const base::FilePath& cur_dir,
    const base::CommandLine& command_line,
    bool ignore_profile_picker) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const base::FilePath& user_data_dir = profile_manager->user_data_dir();
// If the browser is launched due to activation on Windows native notification,
// the profile id encoded in the notification launch id should be chosen over
// all others.
#if BUILDFLAG(IS_WIN)
  base::FilePath profile_basename =
      NotificationLaunchId::GetNotificationLaunchProfileBaseName(command_line);
  if (!profile_basename.empty()) {
    return {.path = user_data_dir.Append(profile_basename),
            .reason = StartupProfileModeReason::kNotificationLaunchIdWin1};
  }
#endif  // BUILDFLAG(IS_WIN)

  // If opening in Guest mode is requested, load the default profile so that
  // last opened profile would not trigger a user management dialog.
  if (profiles::IsGuestModeRequested(command_line,
                                     g_browser_process->local_state(),
                                     /* show_warning= */ false)) {
    // TODO(crbug.com/40157821): return a guest profile instead.
    return {.path = profiles::GetDefaultProfileDir(user_data_dir),
            .reason = StartupProfileModeReason::kGuestModeRequested};
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (chromeos::BrowserParamsProxy::Get()->SessionType() ==
      crosapi::mojom::SessionType::kGuestSession) {
    return {.path = ProfileManager::GetGuestProfilePath(),
            .reason = StartupProfileModeReason::kGuestSessionLacros};
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  base::FilePath command_line_profile_directory =
      command_line.GetSwitchValuePath(switches::kProfileDirectory);

  if (!command_line_profile_directory.empty() &&
      command_line.HasSwitch(switches::kIgnoreProfileDirectoryIfNotExists)) {
    base::FilePath profile_dir_path =
        user_data_dir.Append(command_line_profile_directory);
    // This is a blocking call to the filesystem, but unfortunately it is
    // required for startup to continue, as the
    // `kIgnoreProfileDirectoryIfNotExists` switch needs to check the file
    // system state to know if the profile directory exists.
    base::ScopedAllowBlocking allow_blocking;
    if (IsProfileDirectoryMarkedForDeletion(profile_dir_path) ||
        !base::DirectoryExists(profile_dir_path)) {
      command_line_profile_directory = base::FilePath();
    }
  }

#if BUILDFLAG(IS_MAC)
  // On Mac OS, when an app shim fails to dlopen chrome, the app shim falls back
  // to trying to launch chrome passing its app-id on the command line. In this
  // situation, the profile directory passed on the command line could be an
  // empty string. When this happens, arbitrarily pick one of the profiles the
  // app is installed in as profile directory. While this isn't necesarilly the
  // right profile to use, it is good enough for the purpose of (indirectly)
  // triggering a rebuild of the app shim, which should resolve whatever
  // problem existed that let to this situation.
  if (command_line_profile_directory.empty() &&
      command_line.HasSwitch(switches::kAppId)) {
    std::string app_id = command_line.GetSwitchValueASCII(switches::kAppId);
    std::set<base::FilePath> profile_paths =
        AppShimRegistry::Get()->GetInstalledProfilesForApp(app_id);
    if (!profile_paths.empty()) {
      command_line_profile_directory = profile_paths.begin()->BaseName();
    }
  }
#endif
  if (!command_line_profile_directory.empty()) {
    return {.path = user_data_dir.Append(command_line_profile_directory),
            .reason = StartupProfileModeReason::kProfileDirSwitch};
  }

  if (command_line.HasSwitch(switches::kProfileEmail)) {
    // Use GetSwitchValueNative() rather than GetSwitchValueASCII() to support
    // non-ASCII email addresses.
    base::CommandLine::StringType email_native =
        command_line.GetSwitchValueNative(switches::kProfileEmail);
    if (!email_native.empty()) {
      std::string email;
#if BUILDFLAG(IS_WIN)
      email = base::WideToUTF8(email_native);
#else
      email = std::move(email_native);
#endif
      base::FilePath profile_dir =
          g_browser_process->profile_manager()->GetProfileDirForEmail(email);
      if (!profile_dir.empty()) {
        return {.path = profile_dir,
                .reason = StartupProfileModeReason::kProfileEmailSwitch};
      }
    }
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return {.path = profile_manager->GetLastUsedProfileDir(),
          .reason = StartupProfileModeReason::kPickerNotSupported};
#else
  if (ignore_profile_picker) {
    return {.path = profile_manager->GetLastUsedProfileDir(),
            .reason = StartupProfileModeReason::kIgnoreProfilePicker};
  }

  // Open the picker only if no URLs have been provided to launch Chrome. If
  // URLs are provided or if we aren't able to extract them at this stage (e.g.
  // we need a profile to access search engine preferences and attempt to
  // resolve a query into a URL), open them in the last profile, instead.
  auto has_tabs =
      StartupTabProviderImpl().HasCommandLineTabs(command_line, cur_dir);
  if (has_tabs != CommandLineTabsPresent::kNo) {
    return {.path = profile_manager->GetLastUsedProfileDir(),
            .reason = StartupProfileModeReason::kCommandLineTabs};
  }

  StartupProfileModeReason show_picker_reason =
      ShouldShowProfilePickerAtProcessLaunch(
          profile_manager, !command_line_profile_directory.empty(),
          command_line);

  if (StartupProfileModeFromReason(show_picker_reason) ==
      StartupProfileMode::kProfilePicker) {
    return {.path = base::FilePath(), .reason = show_picker_reason};
  }

  return {.path = profile_manager->GetLastUsedProfileDir(),
          .reason = show_picker_reason};
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
StartupProfileInfo GetStartupProfile(const base::FilePath& cur_dir,
                                     const base::CommandLine& command_line) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  StartupProfilePathInfo path_info = GetStartupProfilePath(
      cur_dir, command_line, /*ignore_profile_picker=*/false);
  DCHECK_NE(path_info.reason, StartupProfileModeReason::kError);
  StartupProfileMode mode = StartupProfileModeFromReason(path_info.reason);
  base::UmaHistogramEnumeration("ProfilePicker.StartupMode.GetStartupProfile",
                                mode);
  base::UmaHistogramEnumeration("ProfilePicker.StartupReason.GetStartupProfile",
                                path_info.reason);

  switch (mode) {
    case StartupProfileMode::kProfilePicker:
      return GetProfilePickerStartupProfileInfo();
    case StartupProfileMode::kError:
      // No more info to add.
      return {nullptr, StartupProfileMode::kError};
    case StartupProfileMode::kBrowserWindow:
      // Try to acquire a profile below.
      break;
  }

  // NOTE: GetProfile() does synchronous file I/O on the main thread.
  Profile* profile = profile_manager->GetProfile(path_info.path);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (profile->IsGuestSession()) {
    return {profile, StartupProfileMode::kBrowserWindow};
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // There are several cases where we should show the profile picker:
  // - if there is no entry in profile attributes storage, which means that the
  //   profile is deleted,
  // - if the profile is locked,
  // - if the profile has failed to load
  // When neither of these is true, we can safely start up with `profile`.
  auto* storage = &profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage->GetProfileAttributesWithPath(path_info.path);
  if (entry && !entry->IsSigninRequired() && profile) {
    return {profile, StartupProfileMode::kBrowserWindow};
  }

  return GetProfilePickerStartupProfileInfo();
}

StartupProfileInfo GetFallbackStartupProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // The only known reason for profiles to fail initialization is being unable
  // to create the profile directory, and this has already happened in
  // GetStartupProfilePath() before calling this function. In this case,
  // creation of new profiles is expected to fail. So only existing profiles are
  // attempted for fallback.

  // If the last used profile could not be initialized, see if any of other last
  // opened profiles can be initialized successfully.
  auto* storage = &profile_manager->GetProfileAttributesStorage();
  for (Profile* profile : ProfileManager::GetLastOpenedProfiles()) {
    // Return any profile that is not locked.
    ProfileAttributesEntry* entry =
        storage->GetProfileAttributesWithPath(profile->GetPath());
    if (!entry || !entry->IsSigninRequired())
      return {profile, StartupProfileMode::kBrowserWindow};
  }

  // Couldn't initialize any last opened profiles. Try to show the profile
  // picker.
  StartupProfileInfo profile_picker_info = GetProfilePickerStartupProfileInfo();
  if (profile_picker_info.mode != StartupProfileMode::kError)
    return profile_picker_info;

  // Couldn't show the profile picker either. Try to open any profile that is
  // not locked.
  for (ProfileAttributesEntry* entry : storage->GetAllProfilesAttributes()) {
    if (!entry->IsSigninRequired()) {
      Profile* profile = profile_manager->GetProfile(entry->GetPath());
      if (profile)
        return {profile, StartupProfileMode::kBrowserWindow};
    }
  }

  return {nullptr, StartupProfileMode::kError};
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
