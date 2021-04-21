// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>

#include "apps/switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/statistics_recorder.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
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
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/extensions/startup_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/web_app_ui_manager_impl.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/switches.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/app_mode/app_launch_utils.h"
#include "chrome/browser/chromeos/full_restore/full_restore_service.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/extensions/api/messaging/native_messaging_launch_from_native.h"
#include "chrome/browser/ui/profile_picker.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#endif

#if defined(TOOLKIT_VIEWS) && defined(USE_X11)
#include "ui/events/devices/x11/touch_factory_x11.h"  // nogncheck
#endif

#if defined(OS_MAC)
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut_mac.h"
#endif

#if defined(OS_WIN)
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/metrics/jumplist_metrics_win.h"
#include "chrome/browser/notifications/notification_platform_bridge_win.h"
#include "chrome/browser/notifications/win/notification_launch_id.h"
#include "chrome/browser/ui/startup/credential_provider_signin_dialog_win.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/print_dialog_cloud_win.h"
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // defined(OS_WIN)

#if defined(USE_X11)
#include "ui/base/ui_base_features.h"
#endif

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
#include "chrome/browser/web_applications/components/url_handler_launch_params.h"
#include "chrome/browser/web_applications/components/url_handler_manager_impl.h"
#include "third_party/blink/public/common/features.h"
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
    observed_profiles_.Remove(profile);
    launched_profiles_.erase(profile);
    opened_profiles_.erase(profile);
    if (profile == profile_to_activate_)
      profile_to_activate_ = nullptr;
    // If this profile was the last launched one without an opened window,
    // then we may be ready to activate |profile_to_activate_|.
    MaybeActivateProfile();
  }

  bool HasBeenLaunched(const Profile* profile) const {
    return launched_profiles_.find(profile) != launched_profiles_.end();
  }

  void AddLaunched(Profile* profile) {
    if (!observed_profiles_.IsObserving(profile))
      observed_profiles_.Add(profile);
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
    if (!observed_profiles_.IsObserving(profile))
      observed_profiles_.Add(profile);
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
    observed_profiles_.RemoveAll();
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
  std::set<const Profile*> launched_profiles_;
  // These are the profiles for which at least one browser window has been
  // opened. This is needed to know when it is safe to activate
  // |profile_to_activate_|, otherwise, new browser windows being opened will
  // be activated on top of it.
  std::set<const Profile*> opened_profiles_;
  // This is null until the profile to activate has been chosen. This value
  // should only be set once all profiles have been launched, otherwise,
  // activation may not happen after the launch of newer profiles.
  Profile* profile_to_activate_ = nullptr;
  // Set once we attempted to activate a profile. We only get one shot at this.
  bool activated_profile_ = false;
  ScopedObserver<Profile, ProfileObserver> observed_profiles_{this};
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
  base::WriteFile(output_file, output_string.data(),
                  static_cast<int>(output_string.size()));
}

// Returns whether |profile| can be opened during Chrome startup without
// explicit user action.
bool CanOpenProfileOnStartup(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS, the user has already chosen and logged into the profile before
  // Chrome starts up.
  return true;
#else
  // Profiles that require signin are not available.
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (entry && entry->IsSigninRequired()) {
    return false;
  }

  // Guest or system profiles are not available unless a separate process
  // already has a window open for the profile.
  if (profile->IsEphemeralGuestProfile())
    return chrome::GetBrowserCount(profile->GetOriginalProfile()) > 0;

  return (!profile->IsGuestSession() && !profile->IsSystemProfile()) ||
         (chrome::GetBrowserCount(profile->GetPrimaryOTRProfile()) > 0);
#endif
}

bool ShouldShowProfilePickerAtProcessLaunch(
    ProfileManager* profile_manager,
    const base::CommandLine& command_line) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return false;
#else
  // Don't show the picker if a certain profile (or an incognito window in the
  // default profile) is explicitly requested.
  if (profiles::IsGuestModeRequested(command_line,
                                     g_browser_process->local_state(),
                                     /*show_warning=*/false) ||
      command_line.HasSwitch(switches::kIncognito) ||
      command_line.HasSwitch(switches::kProfileDirectory)) {
    return false;
  }

  // Don't show the picker if an app is explicitly requested to open. This URL
  // param should be ideally paired with switches::kProfileDirectory but it's
  // better to err on the side of opening the last profile than to err on the
  // side of not opening the app directly.
  if (command_line.HasSwitch(switches::kApp) ||
      command_line.HasSwitch(switches::kAppId)) {
    return false;
  }

// If the browser is launched due to activation on Windows native notification,
// the profile id encoded in the notification launch id should be chosen over
// the profile picker.
#if defined(OS_WIN)
  std::string profile_id =
      NotificationLaunchId::GetNotificationLaunchProfileId(command_line);
  if (!profile_id.empty()) {
    return false;
  }
#endif  // defined(OS_WIN)
  return ProfilePicker::ShouldShowAtLaunch();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShowProfilePicker(bool is_process_startup) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    ProfilePicker::Show(
        is_process_startup
            ? ProfilePicker::EntryPoint::kOnStartup
            : ProfilePicker::EntryPoint::kNewSessionOnExistingProcess);
    return;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

bool IsSilentLaunchEnabled(const base::CommandLine& command_line,
                           const Profile* profile) {
  // Note: This check should have been done in ProcessCmdLineImpl()
  // before calling this function. However chromeos/login/login_utils.cc
  // calls this function directly (see comments there) so it has to be checked
  // again.

  if (command_line.HasSwitch(switches::kSilentLaunch))
    return true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return profile->GetPrefs()->GetBoolean(
      prefs::kStartupBrowserWindowLaunchSuppressed);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return false;
}

void FinalizeWebAppLaunch(
    std::unique_ptr<LaunchModeRecorder> launch_mode_recorder,
    Browser* browser,
    apps::mojom::LaunchContainer container) {
  LaunchMode mode;
  switch (container) {
    case apps::mojom::LaunchContainer::kLaunchContainerWindow:
      DCHECK(browser->is_type_app());
      mode = LaunchMode::kAsWebAppInWindow;
      break;
    case apps::mojom::LaunchContainer::kLaunchContainerTab:
      DCHECK(!browser->is_type_app());
      mode = LaunchMode::kAsWebAppInTab;
      break;
    case apps::mojom::LaunchContainer::kLaunchContainerPanelDeprecated:
      NOTREACHED();
      FALLTHROUGH;
    case apps::mojom::LaunchContainer::kLaunchContainerNone:
      DCHECK(!browser->is_type_app());
      mode = LaunchMode::kUnknownWebApp;
      break;
  }

  if (launch_mode_recorder)
    launch_mode_recorder->SetLaunchMode(mode);
  StartupBrowserCreatorImpl::MaybeToggleFullscreen(browser);
}

// Tries to get the protocol url from the command line.
// If the protocol app url switch doesnt exist, checks if the passed in url
// is a potential protocol url, if it is, check the protocol handler registry
// for an entry. Return the protocol url if there are handlers for this scheme.
bool MaybeLaunchProtocolHandlerWebApp(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    std::unique_ptr<LaunchModeRecorder> launch_mode_recorder) {
  // Maybe the URL passed in is a protocol URL.
  GURL protocol_url;
  base::CommandLine::StringVector args = command_line.GetArgs();
  for (const auto& arg : args) {
#if defined(OS_WIN)
    GURL potential_protocol(base::WideToUTF16(arg));
#else
    GURL potential_protocol(arg);
#endif  // defined(OS_WIN)
    if (potential_protocol.is_valid() && !potential_protocol.IsStandard()) {
      protocol_url = potential_protocol;
      break;
    }
  }
  if (protocol_url.is_empty())
    return false;

  ProtocolHandlerRegistry* handler_registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(profile);
  const std::vector<ProtocolHandler> handlers =
      handler_registry->GetHandlersFor(protocol_url.scheme());

  // Check that there is at least one handler with web_app_id.
  // TODO(crbug/1019239): Display intent picker if there are multiple handlers.
  for (const auto& handler : handlers) {
    if (handler.web_app_id().has_value()) {
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithCallback(
              handler.web_app_id().value(), command_line, cur_dir,
              /*url_handler_launch_url=*/base::nullopt, protocol_url,
              base::BindOnce(&FinalizeWebAppLaunch,
                             std::move(launch_mode_recorder)));
      return true;
    }
  }

  return false;
}

// If the process was launched with the web application command line flags,
// e.g. --app=http://www.google.com/ or --app_id=... return true.
// In this case |app_url| or |app_id| are populated if they're non-null.
bool IsAppLaunch(const base::CommandLine& command_line,
                 std::string* app_url,
                 std::string* app_id) {
  if (command_line.HasSwitch(switches::kApp)) {
    if (app_url)
      *app_url = command_line.GetSwitchValueASCII(switches::kApp);
    return true;
  }
  if (command_line.HasSwitch(switches::kAppId)) {
    if (app_id)
      *app_id = command_line.GetSwitchValueASCII(switches::kAppId);
    return true;
  }
  return false;
}

// Opens an application window or tab if the process was launched with the web
// application command line switches. Returns true if launch succeeded (or is
// proceeding asynchronously); otherwise, returns false to indicate that
// normal browser startup should resume. Desktop web applications launch
// asynchronously, and fall back to launching a browser window.
bool MaybeLaunchApplication(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    std::unique_ptr<LaunchModeRecorder> launch_mode_recorder) {
  std::string url_string, app_id;
  if (!IsAppLaunch(command_line, &url_string, &app_id))
    return false;

  if (!app_id.empty()) {
    // Opens an empty browser window if the app_id is invalid.
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->BrowserAppLauncher()
        ->LaunchAppWithCallback(
            app_id, command_line, cur_dir,
            /*url_handler_launch_url=*/base::nullopt,
            /*protocol_handler_launch_url=*/base::nullopt,
            base::BindOnce(&FinalizeWebAppLaunch,
                           std::move(launch_mode_recorder)));
    return true;
  }

  if (url_string.empty())
    return false;

#if defined(OS_WIN)  // Fix up Windows shortcuts.
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
        FinalizeWebAppLaunch(
            std::move(launch_mode_recorder),
            chrome::FindBrowserWithWebContents(web_contents),
            apps::mojom::LaunchContainer::kLaunchContainerWindow);
        return true;
      }
    }
  }
  return false;
}

#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
// If |command_line| contains a single URL argument and that URL matches URL
// handling registration from installed web apps, show app options to user and
// launch one if accepted.
// Returns true if launching an app, false otherwise.
bool MaybeLaunchUrlHandlerWebApp(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    std::unique_ptr<LaunchModeRecorder> launch_mode_recorder) {
  if (!base::FeatureList::IsEnabled(blink::features::kWebAppEnableUrlHandlers))
    return false;

  const std::vector<web_app::UrlHandlerLaunchParams> url_handler_matches =
      web_app::UrlHandlerManagerImpl::GetUrlHandlerMatches(command_line);

  // Launch the first match for which a Profile can be loaded.
  // TODO(crbug/1072058): Use WebAppUiManagerImpl and WebAppDialogManager
  // to display the intent picker dialog. Use the first match here for testing.
  // TODO(crbug/1072058): Check user preferences before showing intent picker.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  for (const auto& match : url_handler_matches) {
    // Do not load profile if profile path is not valid.
    if (!profile_manager->GetProfileAttributesStorage()
             .GetProfileAttributesWithPath(match.profile_path)) {
      continue;
    }
    Profile* const profile = profile_manager->GetProfile(match.profile_path);
    if (profile == nullptr)
      continue;

    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->BrowserAppLauncher()
        ->LaunchAppWithCallback(
            match.app_id, command_line, cur_dir, match.url,
            /*protocol_handler_launch_url=*/base::nullopt,
            base::BindOnce(&FinalizeWebAppLaunch,
                           std::move(launch_mode_recorder)));
    return true;
  }
  return false;
}
#endif

}  // namespace

StartupBrowserCreator::StartupBrowserCreator() = default;

StartupBrowserCreator::~StartupBrowserCreator() = default;

// static
bool StartupBrowserCreator::was_restarted_read_ = false;

// static
bool StartupBrowserCreator::in_synchronous_profile_launch_ = false;

void StartupBrowserCreator::AddFirstRunTab(const GURL& url) {
  first_run_tabs_.push_back(url);
}

bool StartupBrowserCreator::Start(const base::CommandLine& cmd_line,
                                  const base::FilePath& cur_dir,
                                  Profile* last_used_profile,
                                  const Profiles& last_opened_profiles) {
  TRACE_EVENT0("startup", "StartupBrowserCreator::Start");
  SCOPED_UMA_HISTOGRAM_TIMER("Startup.StartupBrowserCreator_Start");
  return ProcessCmdLineImpl(cmd_line, cur_dir, true, last_used_profile,
                            last_opened_profiles);
}

// static
bool StartupBrowserCreator::InSynchronousProfileLaunch() {
  return in_synchronous_profile_launch_;
}

Profile* StartupBrowserCreator::GetPrivateProfileIfRequested(
    const base::CommandLine& command_line,
    Profile* profile) {
  if (profiles::IsGuestModeRequested(command_line,
                                     g_browser_process->local_state(),
                                     /* show_warning= */ true)) {
    profile = g_browser_process->profile_manager()->GetProfile(
        ProfileManager::GetGuestProfilePath());
    if (!profile->IsEphemeralGuestProfile())
      profile = profile->GetPrimaryOTRProfile();
    return profile;
  }

  if (IncognitoModePrefs::ShouldLaunchIncognito(command_line,
                                                profile->GetPrefs())) {
    return profile->GetPrimaryOTRProfile();
  } else {
    bool expect_incognito = command_line.HasSwitch(switches::kIncognito);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    auto* init_params = chromeos::LacrosChromeServiceImpl::Get()->init_params();
    // TODO(https://crbug.com/1194304): Remove in M93.
    expect_incognito |= init_params->is_incognito_deprecated;
    expect_incognito |=
        init_params->initial_browser_action ==
        crosapi::mojom::InitialBrowserAction::kOpenIncognitoWindow;
#endif
    LOG_IF(WARNING, expect_incognito)
        << "Incognito mode disabled by policy, launching a normal "
        << "browser session.";
  }

  return profile;
}

bool StartupBrowserCreator::LaunchBrowser(
    const base::CommandLine& command_line,
    Profile* profile,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup process_startup,
    chrome::startup::IsFirstRun is_first_run,
    std::unique_ptr<LaunchModeRecorder> launch_mode_recorder) {
  DCHECK(profile);
#if defined(OS_WIN)
  DCHECK(!command_line.HasSwitch(credential_provider::kGcpwSigninSwitch));
  DCHECK(!command_line.HasSwitch(switches::kNotificationLaunchId));
#endif  // defined(OS_WIN)
  in_synchronous_profile_launch_ =
      process_startup == chrome::startup::IS_PROCESS_STARTUP;

  profile = GetPrivateProfileIfRequested(command_line, profile);

  if (!IsSilentLaunchEnabled(command_line, profile)) {
    StartupBrowserCreatorImpl lwp(cur_dir, command_line, this, is_first_run);
    const std::vector<GURL> urls_to_launch =
        GetURLsFromCommandLine(command_line, cur_dir, profile);
    const bool launched =
        lwp.Launch(profile, urls_to_launch, in_synchronous_profile_launch_,
                   std::move(launch_mode_recorder));
    in_synchronous_profile_launch_ = false;
    if (!launched) {
      LOG(ERROR) << "launch error";
      return false;
    }
  } else {
    in_synchronous_profile_launch_ = false;
  }

  profile_launch_observer.Get().AddLaunched(profile);

  return true;
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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* init_params = chromeos::LacrosChromeServiceImpl::Get()->init_params();
  // TODO(https://crbug.com/1194304): Remove in M93.
  restore_last_session |= init_params->restore_last_session_deprecated;
  restore_last_session |=
      init_params->initial_browser_action ==
      crosapi::mojom::InitialBrowserAction::kRestoreLastSession;
#endif
  if ((restore_last_session || did_restart) && !profile->IsNewProfile()) {
    pref.type = SessionStartupPref::LAST;
  }

  bool is_guest =
      profile->IsGuestSession() || profile->IsEphemeralGuestProfile();

  // A browser starting for a profile being unlocked should always restore.
  if (!is_guest) {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile->GetPath());

    if (entry && entry->IsSigninRequired())
      pref.type = SessionStartupPref::LAST;
  }

  if (pref.type == SessionStartupPref::LAST &&
      (is_guest || profile->IsOffTheRecord())) {
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
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(prefs::kPromotionalTabsEnabled, true);
  registry->RegisterBooleanPref(prefs::kCommandLineFlagSecurityWarningsEnabled,
                                true);
#endif
  registry->RegisterBooleanPref(prefs::kSuppressUnsupportedOSWarning, false);
  registry->RegisterBooleanPref(prefs::kWasRestarted, false);

#if defined(OS_WIN)
  registry->RegisterStringPref(prefs::kShortcutMigrationVersion, std::string());
#endif
}

// static
void StartupBrowserCreator::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Default to true so that existing users are not shown the Welcome page.
  // ProfileManager handles setting this to false for new profiles upon
  // creation.
  registry->RegisterBooleanPref(prefs::kHasSeenWelcomePage, true);
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // This will be set for newly created profiles, and is used to indicate which
  // users went through onboarding with the current experiment group.
  registry->RegisterStringPref(prefs::kNaviOnboardGroup, "");
#endif  // defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

bool StartupBrowserCreator::ProcessCmdLineImpl(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    bool process_startup,
    Profile* last_used_profile,
    const Profiles& last_opened_profiles) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT0("startup", "StartupBrowserCreator::ProcessCmdLineImpl");

  DCHECK(last_used_profile);
  if (process_startup &&
      command_line.HasSwitch(switches::kDisablePromptOnRepost)) {
    content::NavigationController::DisablePromptOnRepost();
  }

  bool silent_launch = false;
  bool can_use_last_profile =
      (CanOpenProfileOnStartup(last_used_profile) &&
       !IncognitoModePrefs::ShouldLaunchIncognito(
           command_line, last_used_profile->GetPrefs()));

#if defined(OS_WIN) && BUILDFLAG(ENABLE_PRINT_PREVIEW)
  // If we are just displaying a print dialog we shouldn't open browser
  // windows.
  if (command_line.HasSwitch(switches::kCloudPrintFile) &&
      can_use_last_profile &&
      print_dialog_cloud::CreatePrintDialogFromCommandLine(last_used_profile,
                                                           command_line)) {
    silent_launch = true;
  }
#endif  // defined(OS_WIN) && BUILDFLAG(ENABLE_PRINT_PREVIEW)

  if (command_line.HasSwitch(switches::kValidateCrx)) {
    if (!process_startup) {
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
  if (command_line.HasSwitch(chromeos::switches::kLoginManager))
    silent_launch = true;

  if (chrome::IsRunningInForcedAppMode()) {
    user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(last_used_profile);
    if (user && user->GetType() == user_manager::USER_TYPE_KIOSK_APP) {
      chromeos::LaunchAppOrDie(
          last_used_profile,
          chromeos::KioskAppId::ForChromeApp(
              command_line.GetSwitchValueASCII(switches::kAppId)));
    } else if (user &&
               user->GetType() == user_manager::USER_TYPE_WEB_KIOSK_APP) {
      chromeos::LaunchAppOrDie(
          last_used_profile,
          chromeos::KioskAppId::ForWebApp(user->GetAccountId()));
    } else {
      // If we are here, we are either in ARC kiosk session or the user is
      // invalid. We should terminate the session in such cases.
      chrome::AttemptUserExit();
      return false;
    }

    // Skip browser launch since app mode launches its app window.
    silent_launch = true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(TOOLKIT_VIEWS) && defined(USE_X11)
  if (!features::IsUsingOzonePlatform()) {
    // Ozone sets the device list upon platform initialisation.
    ui::TouchFactory::SetTouchDeviceListFromCommandLine();
  }
#endif

#if defined(OS_MAC)
  if (web_app::MaybeRebuildShortcut(command_line))
    return true;
#endif

  if (!process_startup &&
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

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (base::FeatureList::IsEnabled(features::kOnConnectNative) &&
      command_line.HasSwitch(switches::kNativeMessagingConnectHost) &&
      command_line.HasSwitch(switches::kNativeMessagingConnectExtension)) {
    extensions::LaunchNativeMessageHostFromNativeApp(
        command_line.GetSwitchValueASCII(
            switches::kNativeMessagingConnectExtension),
        command_line.GetSwitchValueASCII(switches::kNativeMessagingConnectHost),
        command_line.GetSwitchValueASCII(switches::kNativeMessagingConnectId),
        last_used_profile);

    // Chrome's lifetime, if the specified extension and native messaging host
    // are both valid and a connection is established, is prolonged by
    // BackgroundModeManager. If |process_startup| is true, --no-startup-window
    // must be set or a browser window must be created for BackgroundModeManager
    // to start background mode. Without this, nothing will take the first
    // keep-alive and the browser process will not terminate. To avoid this
    // situation, don't set |silent_launch| in response to the native messaging
    // connect switches; require the client to pass --no-startup-window if
    // suppressing the creation of a window is desired.
  }
#endif

  // If --no-startup-window is specified and Chrome is already running then do
  // not open a new window.
  if (!process_startup && command_line.HasSwitch(switches::kNoStartupWindow))
    silent_launch = true;

  // If we don't want to launch a new browser window or tab we are done here.
  if (silent_launch) {
    if (process_startup)
      startup_metric_utils::SetNonBrowserUIDisplayed();
    return true;
  }

#if defined(OS_WIN)
  // If --uninstall-app-id is specified, remove the target web app.
  if (command_line.HasSwitch(switches::kUninstallAppId)) {
    std::string app_id =
        command_line.GetSwitchValueASCII(switches::kUninstallAppId);

    web_app::WebAppUiManagerImpl::Get(last_used_profile)
        ->UninstallWebAppFromStartupSwitch(app_id);

    // Return true to allow startup to continue and for the main event loop to
    // run. The process will shut down if no browser windows are open when the
    // uninstall completes thanks to UninstallWebAppFromStartupSwitch's
    // ScopedKeepAlive.
    return true;
  }
#endif  // defined(OS_WIN)

  if (command_line.HasSwitch(extensions::switches::kLoadApps) &&
      can_use_last_profile) {
    if (!ProcessLoadApps(command_line, cur_dir, last_used_profile))
      return false;

    // Return early here to avoid opening a browser window.
    // The exception is when there are no browser windows, since we don't want
    // chrome to shut down.
    // TODO(jackhou): Do this properly once keep-alive is handled by the
    // background page of apps. Tracked at http://crbug.com/175381
    if (chrome::GetBrowserCount(last_used_profile) != 0)
      return true;
  }

  // Check for --load-and-launch-app.
  if (command_line.HasSwitch(apps::kLoadAndLaunchApp) && can_use_last_profile) {
    base::CommandLine::StringType path =
        command_line.GetSwitchValueNative(apps::kLoadAndLaunchApp);

    if (!apps::AppLoadService::Get(last_used_profile)
             ->LoadAndLaunch(base::FilePath(path), command_line, cur_dir)) {
      return false;
    }

    // Return early here since we don't want to open a browser window.
    // The exception is when there are no browser windows, since we don't want
    // chrome to shut down.
    // TODO(jackhou): Do this properly once keep-alive is handled by the
    // background page of apps. Tracked at http://crbug.com/175381
    if (chrome::GetBrowserCount(last_used_profile) != 0)
      return true;
  }

#if defined(OS_WIN)
  // Log whether this process was a result of an action in the Windows Jumplist.
  if (command_line.HasSwitch(switches::kWinJumplistAction)) {
    jumplist::LogJumplistActionFromSwitchValue(
        command_line.GetSwitchValueASCII(switches::kWinJumplistAction));
    // Use a non-NULL pointer to indicate JumpList has been used. We re-use
    // chrome::kJumpListIconDirname as the key to the data.
    last_used_profile->SetUserData(
        chrome::kJumpListIconDirname,
        base::WrapUnique(new base::SupportsUserData::Data()));
  }

  // If the command line has the kNotificationLaunchId switch, then this
  // call is from notification_helper.exe to process toast activation.
  // Delegate to the notification system; do not open a browser window here.
  if (command_line.HasSwitch(switches::kNotificationLaunchId)) {
    if (NotificationPlatformBridgeWin::HandleActivation(command_line)) {
      LaunchModeRecorder().SetLaunchMode(LaunchMode::kWinPlatformNotification);
      return true;
    }
    return false;
  }

  // If being started for credential provider logon purpose, only show the
  // signin page.
  if (command_line.HasSwitch(credential_provider::kGcpwSigninSwitch)) {
    // Use incognito profile since this is a credential provider logon.
    Profile* profile = last_used_profile->GetPrimaryOTRProfile();
    DCHECK(profile->IsIncognitoProfile());
    // NOTE: All launch urls are ignored when running with --gcpw-signin since
    // this mode only loads Google's sign in page.

    // If GCPW signin dialog fails, returning false here will allow Chrome to
    // exit gracefully during the launch.
    if (!StartGCPWSignin(command_line, profile))
      return false;

    LaunchModeRecorder().SetLaunchMode(LaunchMode::kCredentialProviderSignIn);
    return true;
  }
#endif  // defined(OS_WIN)

  if (command_line.HasSwitch(switches::kAppId)) {
    std::string app_id = command_line.GetSwitchValueASCII(switches::kAppId);
    // If |app_id| is a disabled or terminated platform app we handle it
    // specially here, otherwise it will be handled below.
    if (apps::OpenExtensionApplicationWithReenablePrompt(
            last_used_profile, app_id, command_line, cur_dir)) {
      return true;
    }
  }

  // Web app Protocol handling.
  if (MaybeLaunchProtocolHandlerWebApp(
          command_line, cur_dir, last_used_profile,
          std::make_unique<LaunchModeRecorder>())) {
    return true;
  }

  // If we're being run as an application window or application tab, don't
  // restore tabs or open initial URLs as the user has directly launched an app
  // shortcut. In the first case, the user should see a standlone app window. In
  // the second case, the tab should either open in an existing Chrome window
  // for this profile, or spawn a new Chrome window without any NTP if no window
  // exists (see crbug.com/528385).
  // TODO(https://crbug.com/1188873): Investigate switching to private mode
  // sooner if request.
  Profile* profile_to_launch =
      GetPrivateProfileIfRequested(command_line, last_used_profile);
  if (MaybeLaunchApplication(command_line, cur_dir, profile_to_launch,
                             std::make_unique<LaunchModeRecorder>())) {
    // At this point we've opened the app. As a temporary fix for
    // https://crbug.com/1199203, if this startup is also from an unclean exit
    // we also need to open a blank browser window so that users have the
    // opportunity to restore, but also to prevent a potential crash loop.
    // To achieve that, stop this from returning here, and allow it to continue
    // to hit a standard crash reopen codepath and show an empty browser window
    // with the restore dialog.
    if (!HasPendingUncleanExit(profile_to_launch))
      return true;
  }

  // Web app URL handling.
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
  if (MaybeLaunchUrlHandlerWebApp(command_line, cur_dir,
                                  std::make_unique<LaunchModeRecorder>())) {
    return true;
  }
#endif

  return LaunchBrowserForLastProfiles(command_line, cur_dir, process_startup,
                                      last_used_profile, last_opened_profiles);
}

bool StartupBrowserCreator::LaunchBrowserForLastProfiles(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    bool process_startup,
    Profile* last_used_profile,
    const Profiles& last_opened_profiles) {
  chrome::startup::IsProcessStartup is_process_startup =
      process_startup ? chrome::startup::IS_PROCESS_STARTUP
                      : chrome::startup::IS_NOT_PROCESS_STARTUP;
  chrome::startup::IsFirstRun is_first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IS_FIRST_RUN
                                    : chrome::startup::IS_NOT_FIRST_RUN;

  // On Windows, when chrome is launched by notification activation where the
  // kNotificationLaunchId switch is used, always use |last_used_profile| which
  // contains the profile id extracted from the notification launch id.
  bool was_windows_notification_launch = false;
#if defined(OS_WIN)
  was_windows_notification_launch =
      command_line.HasSwitch(switches::kNotificationLaunchId);
#endif  // defined(OS_WIN)

  // TODO(crbug.com/1150326) Calling ShouldShowProfilePickerAtProcessLaunch()
  // a second time here duplicates the logic to show the profile picker. The
  // decision to show the picker should instead be on the previous call to
  // ShouldShowProfilePickerAtProcessLaunch() issued from
  // GetStartupProfilePath().
  // Ephemeral guest is added here just for symmetry, once we use other ways to
  // indicate that picker should get opened, we can remove both IsGuestSession()
  // and IsEphemeralGuestProfile().
  if (ShouldShowProfilePickerAtProcessLaunch(
          g_browser_process->profile_manager(), command_line) &&
      last_used_profile &&
      (last_used_profile->IsGuestSession() ||
       last_used_profile->IsEphemeralGuestProfile())) {
    // The guest session is used to indicate the the profile picker should be
    // displayed on start-up. See GetStartupProfilePath().
    ShowProfilePicker(/*is_process_startup=*/process_startup);
    return true;
  }

  // |last_opened_profiles| will be empty in the following circumstances:
  // - This is the first launch. |last_used_profile| is the initial profile.
  // - The user exited the browser by closing all windows for all profiles.
  //   |last_used_profile| is the profile which owned the last open window.
  // - Only incognito windows were open when the browser exited.
  //   |last_used_profile| is the last used incognito profile. Restoring it will
  //   create a browser window for the corresponding original profile.
  // - All of the last opened profiles fail to initialize.
  if (last_opened_profiles.empty() || was_windows_notification_launch) {
    if (CanOpenProfileOnStartup(last_used_profile)) {
      Profile* profile_to_open = last_used_profile->IsGuestSession()
                                     ? last_used_profile->GetPrimaryOTRProfile()
                                     : last_used_profile;
#if BUILDFLAG(IS_CHROMEOS_ASH)
      if (process_startup) {
        // If FullRestoreService is available for the profile (i.e. the full
        // restore feature is enabled and the profile is a regular user
        // profile), defer the browser launching to FullRestoreService code.
        auto* full_restore_service =
            chromeos::full_restore::FullRestoreService::GetForProfile(
                profile_to_open);
        if (full_restore_service) {
          full_restore_service->LaunchBrowserWhenReady();
          return true;
        }
      }
#endif
      return LaunchBrowser(command_line, profile_to_open, cur_dir,
                           is_process_startup, is_first_run,
                           std::make_unique<LaunchModeRecorder>());
    }

    // Show ProfilePicker if |last_used_profile| can't be auto opened.
    ShowProfilePicker(/*is_process_startup=*/process_startup);
    return true;
  }
  return ProcessLastOpenedProfiles(command_line, cur_dir, is_process_startup,
                                   is_first_run, last_used_profile,
                                   last_opened_profiles);
}

bool StartupBrowserCreator::ProcessLastOpenedProfiles(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    chrome::startup::IsProcessStartup is_process_startup,
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
    DCHECK(!profile->IsGuestSession() && !profile->IsEphemeralGuestProfile());

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    // Skip any locked profile.
    if (!CanOpenProfileOnStartup(profile))
      continue;

    // Guest profiles should not be reopened on startup. This can happen if
    // the last used profile was a Guest, but other profiles were also open
    // when Chrome was closed. In this case, pick a different open profile
    // to be the active one, since the Guest profile is never added to the
    // list of open profiles.
    if (last_used_profile->IsGuestSession() ||
        last_used_profile->IsEphemeralGuestProfile()) {
      last_used_profile = profile;
    }
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
    if (!LaunchBrowser((profile == last_used_profile)
                           ? command_line
                           : command_line_without_urls,
                       profile, cur_dir, is_process_startup, is_first_run,
                       profile == last_used_profile
                           ? std::make_unique<LaunchModeRecorder>()
                           : nullptr)) {
      return false;
    }
    // We've launched at least one browser.
    is_process_startup = chrome::startup::IS_NOT_PROCESS_STARTUP;
  }

// Set the |last_used_profile| to activate if a browser is launched for at
// least one profile. Otherwise, show UserManager.
// Note that this must be done after all profiles have
// been launched so the observer knows about all profiles to wait before
// activation this one.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (is_process_startup == chrome::startup::IS_PROCESS_STARTUP)
    ShowProfilePicker(/*is_process_startup=*/true);
  else
#endif
    profile_launch_observer.Get().set_profile_to_activate(last_used_profile);
  return true;
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
      base::MakeAbsoluteFilePath(base::FilePath(tokenizer.token()));
  if (!apps::AppLoadService::Get(profile)->LoadAndLaunch(
          app_absolute_dir, command_line, cur_dir)) {
    return false;
  }

  while (tokenizer.GetNext()) {
    app_absolute_dir =
        base::MakeAbsoluteFilePath(base::FilePath(tokenizer.token()));

    if (!apps::AppLoadService::Get(profile)->Load(app_absolute_dir)) {
      return false;
    }
  }

  return true;
}

// static
void StartupBrowserCreator::ProcessCommandLineOnProfileCreated(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    Profile::CreateStatus status) {
  if (status != Profile::CREATE_STATUS_INITIALIZED)
    return;
  StartupBrowserCreator startup_browser_creator;
  startup_browser_creator.ProcessCmdLineImpl(command_line, cur_dir, false,
                                             profile, Profiles());
}

// static
void StartupBrowserCreator::ProcessCommandLineAlreadyRunning(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    const base::FilePath& profile_path) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = profile_manager->GetProfileByPath(profile_path);

  // The profile isn't loaded yet and so needs to be loaded asynchronously.
  if (!profile) {
    profile_manager->CreateProfileAsync(
        profile_path, base::BindRepeating(&ProcessCommandLineOnProfileCreated,
                                          command_line, cur_dir));
    return;
  }
  StartupBrowserCreator startup_browser_creator;
  Profiles last_opened_profiles;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // On ChromeOS multiple profiles doesn't apply.
  // If no browser windows are open, i.e. the browser is being kept alive in
  // background mode or for other processing, restore |last_opened_profiles|.
  if (chrome::GetTotalBrowserCount() == 0)
    last_opened_profiles = profile_manager->GetLastOpenedProfiles();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  startup_browser_creator.ProcessCmdLineImpl(command_line, cur_dir,
                                             /*process_startup=*/false, profile,
                                             last_opened_profiles);
}

// static
void StartupBrowserCreator::OpenStartupPages(Browser* browser,
                                             bool process_startup) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  chrome::startup::IsFirstRun is_first_run =
      first_run::IsChromeFirstRun() ? chrome::startup::IS_FIRST_RUN
                                    : chrome::startup::IS_NOT_FIRST_RUN;
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

std::vector<GURL> GetURLsFromCommandLine(const base::CommandLine& command_line,
                                         const base::FilePath& cur_dir,
                                         Profile* profile) {
  std::vector<GURL> urls;

  const base::CommandLine::StringVector& params = command_line.GetArgs();
  for (size_t i = 0; i < params.size(); ++i) {
    base::FilePath param = base::FilePath(params[i]);
    // Handle Vista way of searching - "? <search-term>"
    if ((param.value().size() > 2) && (param.value()[0] == '?') &&
        (param.value()[1] == ' ')) {
      GURL url(GetDefaultSearchURLForSearchTerms(
          TemplateURLServiceFactory::GetForProfile(profile),
          param.LossyDisplayName().substr(2)));
      if (url.is_valid()) {
        urls.push_back(url);
        continue;
      }
    }

    // Otherwise, fall through to treating it as a URL.

    // This will create a file URL or a regular URL.
    // This call can (in rare circumstances) block the UI thread.
    // Allow it until this bug is fixed.
    //  http://code.google.com/p/chromium/issues/detail?id=60641
    GURL url = GURL(param.MaybeAsASCII());

    // http://crbug.com/371030: Only use URLFixerUpper if we don't have a valid
    // URL, otherwise we will look in the current directory for a file named
    // 'about' if the browser was started with a about:foo argument.
    // http://crbug.com/424991: Always use URLFixerUpper on file:// URLs,
    // otherwise we wouldn't correctly handle '#' in a file name.
    if (!url.is_valid() || url.SchemeIsFile()) {
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      url = url_formatter::FixupRelativeFile(cur_dir, param);
    }
    // Exclude dangerous schemes.
    if (!url.is_valid())
      continue;

    const GURL settings_url = GURL(chrome::kChromeUISettingsURL);
    bool url_points_to_an_approved_settings_page = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    // In ChromeOS, allow any settings page to be specified on the command line.
    url_points_to_an_approved_settings_page =
        url.GetOrigin() == settings_url.GetOrigin();
#else
    // Exposed for external cleaners to offer a settings reset to the
    // user. The allowed URLs must match exactly.
    const GURL reset_settings_url =
        settings_url.Resolve(chrome::kResetProfileSettingsSubPage);
    url_points_to_an_approved_settings_page = url == reset_settings_url;
#if defined(OS_WIN)
    // On Windows, also allow a hash for the Chrome Cleanup Tool.
    const GURL reset_settings_url_with_cct_hash = reset_settings_url.Resolve(
        std::string("#") +
        settings::ResetSettingsHandler::kCctResetSettingsHash);
    url_points_to_an_approved_settings_page =
        url_points_to_an_approved_settings_page ||
        url == reset_settings_url_with_cct_hash;
#endif  // defined(OS_WIN)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

    ChildProcessSecurityPolicy* policy =
        ChildProcessSecurityPolicy::GetInstance();
    if (policy->IsWebSafeScheme(url.scheme()) ||
        url.SchemeIs(url::kFileScheme) ||
        url_points_to_an_approved_settings_page ||
        (url.spec().compare(url::kAboutBlankURL) == 0)) {
      urls.push_back(url);
    }
  }
  return urls;
}

bool HasPendingUncleanExit(Profile* profile) {
  return profile->GetLastSessionExitType() == Profile::EXIT_CRASHED &&
         !profile_launch_observer.Get().HasBeenLaunched(profile);
}

base::FilePath GetStartupProfilePath(const base::FilePath& user_data_dir,
                                     const base::FilePath& cur_dir,
                                     const base::CommandLine& command_line,
                                     bool ignore_profile_picker) {
// If the browser is launched due to activation on Windows native notification,
// the profile id encoded in the notification launch id should be chosen over
// all others.
#if defined(OS_WIN)
  std::string profile_id =
      NotificationLaunchId::GetNotificationLaunchProfileId(command_line);
  if (!profile_id.empty()) {
    return user_data_dir.Append(base::FilePath(base::UTF8ToWide(profile_id)));
  }
#endif  // defined(OS_WIN)

  // If opening in Guest mode is requested, load the default profile so that
  // last opened profile would not trigger a user management dialog.
  if (profiles::IsGuestModeRequested(command_line,
                                     g_browser_process->local_state(),
                                     /* show_warning= */ false)) {
    return profiles::GetDefaultProfileDir(user_data_dir);
  }

  if (command_line.HasSwitch(switches::kProfileDirectory)) {
    return user_data_dir.Append(
        command_line.GetSwitchValuePath(switches::kProfileDirectory));
  }

  ProfileManager* profile_manager = g_browser_process->profile_manager();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!ignore_profile_picker &&
      ShouldShowProfilePickerAtProcessLaunch(profile_manager, command_line)) {
    // Open the picker only if no URLs have been provided to launch Chrome. If
    // URLs are provided, open them in the last profile, instead.
    Profile* guest_profile =
        profile_manager->GetProfile(ProfileManager::GetGuestProfilePath());
    // TODO(crbug.com/1150326): Consider resolving urls_to_launch without any
    // profile so that the guest profile does not need to get created in case
    // some URLs are passed and the picker is not shown in the end.
    const std::vector<GURL> urls_to_launch =
        GetURLsFromCommandLine(command_line, cur_dir, guest_profile);
    if (urls_to_launch.empty() &&
        profile_manager->GetProfile(ProfileManager::GetSystemProfilePath())) {
      // To indicate that we want to show the profile picker, return the guest
      // profile. However, we can only do this if the system profile (where the
      // profile picker lives) also exists (or is creatable).
      // TODO(crbug.com/1150326): Refactor this to indicate more directly that
      // profile picker should be shown (returning an enum, or so).
      return ProfileManager::GetGuestProfilePath();
    }
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  return profile_manager->GetLastUsedProfileDir(user_data_dir);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_ANDROID)
Profile* GetStartupProfile(const base::FilePath& user_data_dir,
                           const base::FilePath& cur_dir,
                           const base::CommandLine& command_line) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path = GetStartupProfilePath(
      user_data_dir, cur_dir, command_line, /*ignore_profile_picker=*/false);
  Profile* profile = profile_manager->GetProfile(profile_path);

  // If there is no entry in profile attributes storage, the profile is deleted,
  // and we should show the user manager. Also, when using
  // --new-profile-management, if the profile is locked we should show the user
  // manager as well. When neither of these is true, we can safely start up with
  // |profile|.
  auto* storage = &profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage->GetProfileAttributesWithPath(profile_path);
  if (entry && (!entry->IsSigninRequired() || !profile)) {
    return profile;
  }

  // We want to show the user manager. To indicate this, return the guest
  // profile. However, we can only do this if the system profile (where the user
  // manager lives) also exists (or is creatable).
  // TODO(crbug.com/1150326): Refactor this to indicate more directly that
  // profile picker should be shown (returning an enum, or so).
  return profile_manager->GetProfile(ProfileManager::GetSystemProfilePath())
             ? profile_manager->GetProfile(
                   ProfileManager::GetGuestProfilePath())
             : nullptr;
}

Profile* GetFallbackStartupProfile() {
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
      return profile;
  }

  // Couldn't initialize any last opened profiles. Try to show the user manager,
  // which requires successful initialization of the guest and system profiles.
  Profile* guest_profile =
      profile_manager->GetProfile(ProfileManager::GetGuestProfilePath());
  Profile* system_profile =
      profile_manager->GetProfile(ProfileManager::GetSystemProfilePath());
  if (guest_profile && system_profile)
    return guest_profile;

  // Couldn't show the user manager either. Try to open any profile that is not
  // locked.
  for (ProfileAttributesEntry* entry : storage->GetAllProfilesAttributes()) {
    if (!entry->IsSigninRequired()) {
      Profile* profile = profile_manager->GetProfile(entry->GetPath());
      if (profile)
        return profile;
    }
  }

  return nullptr;
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_ANDROID)
