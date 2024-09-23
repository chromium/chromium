// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/web_app_startup_utils.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/lifetime/termination_notification.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/startup/infobar_utils.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "url/gurl.h"

namespace web_app {
namespace startup {

namespace {

base::OnceClosure& GetStartupDoneCallback() {
  static base::NoDestructor<base::OnceClosure> instance;
  return *instance;
}

base::OnceClosure& GetBrowserShutdownCompleteCallback() {
  static base::NoDestructor<base::OnceClosure> instance;
  return *instance;
}

// Encapsulates web app startup logic. This object keeps itself alive via ref
// counting, attaching a reference to each callback in its control flow. It will
// be destroyed after a window is created or it has been determined that no
// window should be created.
class StartupWebAppCreator
    : public base::RefCountedThreadSafe<StartupWebAppCreator> {
 public:
  // Factory to create a `StartupWebAppCreator` to handle the given command
  // line. Will return false if this launch will not be handled as a web app
  // launch, or true if it will.
  static bool MaybeHandleWebAppLaunch(
      const base::CommandLine& command_line,
      const base::FilePath& cur_dir,
      Profile* profile,
      chrome::startup::IsFirstRun is_first_run) {
    std::string app_id = command_line.GetSwitchValueASCII(switches::kAppId);
    // There must be a kAppId switch arg in the command line to launch.
    if (app_id.empty())
      return false;

    // Ensure keep alive registry is available and is not shutting down before
    // attempting a web apps launch.
    KeepAliveRegistry* keep_alive_registry = KeepAliveRegistry::GetInstance();
    if (!keep_alive_registry || keep_alive_registry->IsShuttingDown()) {
      return false;
    }

    scoped_refptr<StartupWebAppCreator> web_app_startup =
        base::AdoptRef(new StartupWebAppCreator(command_line, cur_dir, profile,
                                                is_first_run, app_id));
    // Even though the launch commands can be scheduled before the provider is
    // started, there is logic filtering out incorrect file & protocol launches
    // that happens without locks first, and that has to wait until the database
    // is loaded.
    WebAppProvider::GetForWebApps(profile)->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(&StartupWebAppCreator::Start, web_app_startup));
    return true;
  }

  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

 private:
  friend class base::RefCountedThreadSafe<StartupWebAppCreator>;

  enum class LaunchResult {
    kHandled,
    kNotHandled,
  };

  StartupWebAppCreator(const base::CommandLine& command_line,
                       const base::FilePath& cur_dir,
                       Profile* profile,
                       chrome::startup::IsFirstRun is_first_run,
                       const webapps::AppId& app_id)
      : command_line_(command_line),
        cur_dir_(cur_dir),
        profile_(profile),
        is_first_run_(is_first_run),
        app_id_(app_id),
        provider_(WebAppProvider::GetForWebApps(profile_)),
        profile_keep_alive_(std::make_unique<ScopedProfileKeepAlive>(
            profile,
            ProfileKeepAliveOrigin::kWebAppPermissionDialogWindow)),
        keep_alive_(std::make_unique<ScopedKeepAlive>(
            KeepAliveOrigin::WEB_APP_INTENT_PICKER,
            KeepAliveRestartOption::DISABLED)),
        subscription_(browser_shutdown::AddAppTerminatingCallback(
            base::BindOnce(&StartupWebAppCreator::OnBrowserShutdown,
                           base::Unretained(this)))) {
    DCHECK(provider_);
  }

  ~StartupWebAppCreator() {
    auto startup_done = std::move(GetStartupDoneCallback());
    if (startup_done)
      std::move(startup_done).Run();
  }

  void Start() {
    if (MaybeLaunchProtocolHandler() == LaunchResult::kHandled)
      return;

    DCHECK(protocol_url_.is_empty());

    if (MaybeLaunchFileHandler() == LaunchResult::kHandled)
      return;

    DCHECK(file_launch_infos_.empty());

    open_mode_ = OpenMode::kInWindowByAppId;

    // Fall back to a normal app launch. This opens an empty browser window if
    // the app_id is invalid.
    LaunchApp();
  }

  void LaunchApp() {
    if (file_launch_infos_.empty()) {
      std::optional<GURL> protocol;
      if (!protocol_url_.is_empty())
        protocol = protocol_url_;
      provider_->scheduler().LaunchApp(
          app_id_, command_line_, cur_dir_,
          /*url_handler_launch_url=*/std::nullopt, protocol,
          /*file_launch_url=*/std::nullopt, /*launch_files=*/{},
          base::BindOnce(&StartupWebAppCreator::OnAppLaunched,
                         base::WrapRefCounted(this)));
      return;
    }

    for (const auto& [url, paths] : file_launch_infos_) {
      provider_->scheduler().LaunchApp(
          app_id_, command_line_, cur_dir_,
          /*url_handler_launch_url=*/std::nullopt,
          /*protocol_handler_launch_url=*/std::nullopt,
          /*file_launch_url=*/url, /*launch_files=*/paths,
          base::BindOnce(&StartupWebAppCreator::OnAppLaunched,
                         base::WrapRefCounted(this)));
    }
  }

  // Determines if the launch is a protocol handler launch. If so, takes
  // responsibility for the rest of the launch process.
  LaunchResult MaybeLaunchProtocolHandler() {
    GURL protocol_url;
    base::CommandLine::StringVector args = command_line_.GetArgs();
    for (const auto& arg : args) {
#if BUILDFLAG(IS_WIN)
      GURL potential_protocol(base::AsStringPiece16(arg));
#else
      GURL potential_protocol(arg);
#endif  // BUILDFLAG(IS_WIN)
      // protocol_url is checked for validity later with getting the provider
      // and consulting the os_integration_manager. However because that process
      // has a wait for "on_registry_ready()", `potential_protocol` checks for
      // blink::IsValidCustomHandlerScheme() here to avoid loading the
      // WebAppProvider with a false positive.
      if (potential_protocol.is_valid() &&
          blink::IsValidCustomHandlerScheme(
              potential_protocol.scheme(),
              blink::ProtocolHandlerSecurityLevel::kStrict)) {
        protocol_url = std::move(potential_protocol);
        break;
      }
    }
    if (protocol_url.is_empty())
      return LaunchResult::kNotHandled;

    // Check if the user has already disallowed this app to launch the protocol.
    // This check takes priority over checking if the protocol is handled
    // by the application.
    WebAppProvider* const provider = WebAppProvider::GetForWebApps(profile_);
    DCHECK(provider->on_registry_ready().is_signaled());
    WebAppRegistrar& registrar = provider->registrar_unsafe();
    if (registrar.IsDisallowedLaunchProtocol(app_id_, protocol_url.scheme())) {
      // If disallowed, return `kHandled` to signal that the launch is spoken
      // for, but do not launch a browser or app window. `this` will be deleted.
      return LaunchResult::kHandled;
    }

    // Check if this app has registered as a handler for the protocol.
    if (!registrar.IsRegisteredLaunchProtocol(app_id_, protocol_url.scheme()))
      return LaunchResult::kNotHandled;

    protocol_url_ = protocol_url;

    // `this` will stay alive until `launch_callback` is executed or destroyed.
    auto launch_callback =
        base::BindOnce(&StartupWebAppCreator::OnUserDecisionDialogCompleted,
                       base::WrapRefCounted(this));

    // Check if we have permission to launch the app directly.
    if (registrar.IsAllowedLaunchProtocol(app_id_, protocol_url_.scheme())) {
      std::move(launch_callback)
          .Run(/*allowed=*/true, /*remember_user_choice=*/false);
    } else {
      ShowWebAppProtocolLaunchDialog(protocol_url_, profile_, app_id_,
                                     std::move(launch_callback));
    }
    return LaunchResult::kHandled;
  }

  // Determines if the launch is a file handler launch. If so, takes
  // responsibility for the rest of the launch process.
  LaunchResult MaybeLaunchFileHandler() {
    std::vector<base::FilePath> launch_files =
        apps::GetLaunchFilesFromCommandLine(command_line_);
    if (launch_files.empty())
      return LaunchResult::kNotHandled;

    file_launch_infos_ = provider_->os_integration_manager()
                             .file_handler_manager()
                             .GetMatchingFileHandlerUrls(app_id_, launch_files);
    if (file_launch_infos_.empty())
      return LaunchResult::kNotHandled;

    const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id_);
    DCHECK(web_app);

    // `this` will stay alive until `launch_callback` is executed or destroyed.
    auto launch_callback =
        base::BindOnce(&StartupWebAppCreator::OnUserDecisionDialogCompleted,
                       base::WrapRefCounted(this));

    switch (web_app->file_handler_approval_state()) {
      case ApiApprovalState::kRequiresPrompt:
        ShowWebAppFileLaunchDialog(launch_files, profile_, app_id_,
                                   std::move(launch_callback));
        break;
      case ApiApprovalState::kAllowed:
        std::move(launch_callback)
            .Run(/*allowed=*/true, /*remember_user_choice=*/false);
        break;
      case ApiApprovalState::kDisallowed:
        // The disallowed case should have been handled by
        // `GetMatchingFileHandlerURL()`.
        NOTREACHED_IN_MIGRATION();
        break;
    }
    return LaunchResult::kHandled;
  }

  void OnPersistUserChoiceCompleted(bool allowed) {
    if (allowed)
      LaunchApp();
    // `this` will be deleted.
  }

  void OnUserDecisionDialogCompleted(bool allowed, bool remember_user_choice) {
    // `this` will stay alive until `persist_callback` is executed or destroyed.
    auto persist_callback =
        base::BindOnce(&StartupWebAppCreator::OnPersistUserChoiceCompleted,
                       base::WrapRefCounted(this), allowed);

    if (remember_user_choice) {
      if (!protocol_url_.is_empty()) {
        ApiApprovalState approval_state = allowed
                                              ? ApiApprovalState::kAllowed
                                              : ApiApprovalState::kDisallowed;
        provider_->scheduler().UpdateProtocolHandlerUserApproval(
            app_id_, protocol_url_.scheme(), approval_state,
            std::move(persist_callback));
      } else {
        DCHECK(!file_launch_infos_.empty());
        provider_->scheduler().PersistFileHandlersUserChoice(
            app_id_, allowed, std::move(persist_callback));
      }
    } else {
      std::move(persist_callback).Run();
    }
  }

  void OnAppLaunched(base::WeakPtr<Browser> browser,
                     base::WeakPtr<content::WebContents> web_contents,
                     apps::LaunchContainer container) {
    // The finalization step should only occur for the first app launch.
    if (app_window_has_been_launched_)
      return;

    FinalizeWebAppLaunch(open_mode_, command_line_, is_first_run_,
                         browser.get(), container);
    app_window_has_been_launched_ = true;
  }

  void OnBrowserShutdown() {
    profile_keep_alive_.reset();
    keep_alive_.reset();

    auto browser_shutdown_complete =
        std::move(GetBrowserShutdownCompleteCallback());
    if (browser_shutdown_complete) {
      CHECK_IS_TEST();
      std::move(browser_shutdown_complete).Run();
    }
  }

  // Command line for this launch.
  const base::CommandLine command_line_;
  const base::FilePath cur_dir_;
  const raw_ptr<Profile> profile_;
  chrome::startup::IsFirstRun is_first_run_;

  // The app id for this launch, corresponding to --app-id on the command line.
  const webapps::AppId app_id_;

  raw_ptr<WebAppProvider> provider_;

  // This object keeps the profile and browser process alive while determining
  // whether to launch a window.
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // Registration for AddAppTerminatingCallback().
  base::CallbackListSubscription subscription_;

  std::optional<OpenMode> open_mode_;

  // At most one of the following members should be non-empty.
  // If non-empty, this launch will be treated as a protocol handler launch.
  GURL protocol_url_;

  // If non-empty, this launch will be treated as a file handler launch.
  WebAppFileHandlerManager::LaunchInfos file_launch_infos_;

  // True after at least one app window has been launched.
  bool app_window_has_been_launched_ = false;
};

}  // namespace

bool MaybeHandleWebAppLaunch(const base::CommandLine& command_line,
                             const base::FilePath& cur_dir,
                             Profile* profile,
                             chrome::startup::IsFirstRun is_first_run) {
  return StartupWebAppCreator::MaybeHandleWebAppLaunch(command_line, cur_dir,
                                                       profile, is_first_run);
}

void FinalizeWebAppLaunch(std::optional<OpenMode> app_open_mode,
                          const base::CommandLine& command_line,
                          chrome::startup::IsFirstRun is_first_run,
                          Browser* browser,
                          apps::LaunchContainer container) {
  if (!browser)
    return;

  OpenMode mode = OpenMode::kUnknown;

  switch (container) {
    case apps::LaunchContainer::kLaunchContainerWindow:
      DCHECK(browser->is_type_app());
      mode = app_open_mode.value_or(OpenMode::kInWindowOther);
      break;
    case apps::LaunchContainer::kLaunchContainerTab:
      DCHECK(!browser->is_type_app());
      mode = OpenMode::kInTab;
      break;
    case apps::LaunchContainer::kLaunchContainerPanelDeprecated:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case apps::LaunchContainer::kLaunchContainerNone:
      DCHECK(!browser->is_type_app());
      break;
  }

  // Log in a histogram the different ways web apps are opened. See
  // OpenMode enum for the values of the buckets.
  base::UmaHistogramEnumeration("WebApp.OpenMode", mode);

  AddInfoBarsIfNecessary(browser, browser->profile(), command_line,
                         is_first_run,
                         /*is_web_app=*/true);

  StartupBrowserCreatorImpl::MaybeToggleFullscreen(browser);
}

void SetStartupDoneCallbackForTesting(base::OnceClosure callback) {
  GetStartupDoneCallback() = std::move(callback);
}

void SetBrowserShutdownCompleteCallbackForTesting(base::OnceClosure callback) {
  CHECK_IS_TEST();
  GetBrowserShutdownCompleteCallback() = std::move(callback);
}

}  // namespace startup
}  // namespace web_app
