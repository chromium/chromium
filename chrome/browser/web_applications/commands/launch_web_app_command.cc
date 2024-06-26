// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/launch_web_app_command.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/concurrent_closures.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace web_app {

LaunchWebAppCommand::LaunchWebAppCommand(
    Profile* profile,
    WebAppProvider* provider,
    apps::AppLaunchParams params,
    LaunchWebAppWindowSetting launch_setting,
    LaunchWebAppCallback callback)
    : WebAppCommand<AppLock,
                    base::WeakPtr<Browser>,
                    base::WeakPtr<content::WebContents>,
                    apps::LaunchContainer>(
          "LaunchWebAppCommand",
          AppLockDescription(params.app_id),
          std::move(callback),
          /*args_for_shutdown=*/
          std::make_tuple(nullptr,
                          nullptr,
                          apps::LaunchContainer::kLaunchContainerNone)),
      params_(std::move(params)),
      launch_setting_(launch_setting),
      profile_(*profile),
      provider_(*provider) {
  CHECK(provider);
}

LaunchWebAppCommand::~LaunchWebAppCommand() = default;

void LaunchWebAppCommand::StartWithLock(std::unique_ptr<AppLock> lock) {
  lock_ = std::move(lock);
  if (!lock_->registrar().IsInstalled(params_.app_id)) {
    GetMutableDebugValue().Set("error", "not_installed");
    CompleteAndSelfDestruct(CommandResult::kFailure, nullptr, nullptr,
                            apps::LaunchContainer::kLaunchContainerNone);
    return;
  }

  bool is_standalone_launch =
      params_.container == apps::LaunchContainer::kLaunchContainerWindow ||
      (launch_setting_ ==
           LaunchWebAppWindowSetting::kOverrideWithWebAppConfig &&
       lock_->registrar().GetAppUserDisplayMode(params_.app_id) !=
           mojom::UserDisplayMode::kBrowser);

  GetMutableDebugValue().Set("is_standalone_launch", is_standalone_launch);
  if (is_standalone_launch) {
    // Launching an app in a standalone windows requires OS integration, and the
    // only way this is supported in tests is to use the
    // OsIntegrationTestOverride functionality.
    CHECK_OS_INTEGRATION_ALLOWED();
  }

  bool needs_os_integration_sync = false;

  // Upgrade to fully installed if needed.
  if (is_standalone_launch &&
      lock_->registrar().GetInstallState(params_.app_id) !=
          proto::INSTALLED_WITH_OS_INTEGRATION) {
    ScopedRegistryUpdate update = lock_->sync_bridge().BeginUpdate();
    update->UpdateApp(params_.app_id)
        ->SetInstallState(proto::INSTALLED_WITH_OS_INTEGRATION);
    needs_os_integration_sync = true;
  }

  std::optional<proto::WebAppOsIntegrationState> os_integration =
      lock_->registrar().GetAppCurrentOsIntegrationState(params_.app_id);
  CHECK(os_integration);
  GetMutableDebugValue().Set("needs_os_integration_sync",
                             needs_os_integration_sync);

  base::ConcurrentClosures completion;

  if (needs_os_integration_sync) {
    // TODO(crbug.com/339451551): Remove adding to desktop on linux after the
    // OsIntegrationTestOverride can use the xdg install command to detect
    // install.
    SynchronizeOsOptions options;
#if BUILDFLAG(IS_LINUX)
    options.add_shortcut_to_desktop = true;
#endif
    lock_->os_integration_manager().Synchronize(
        params_.app_id,
        base::BindOnce(&LaunchWebAppCommand::OnOsIntegrationSynchronized,
                       weak_factory_.GetWeakPtr())
            .Then(completion.CreateClosure()),
        options);
  }

  // Note: In tests this can synchronously call FirstRunServiceCompleted and
  // self-destruct. So take the weak pointer first.
  base::WeakPtr<LaunchWebAppCommand> weak_ptr = weak_factory_.GetWeakPtr();
  provider_->ui_manager().WaitForFirstRunService(
      *profile_, base::BindOnce(&LaunchWebAppCommand::FirstRunServiceCompleted,
                                weak_factory_.GetWeakPtr())
                     .Then(completion.CreateClosure()));

  std::move(completion)
      .Done(base::BindOnce(&LaunchWebAppCommand::DoLaunch, weak_ptr));
}

void LaunchWebAppCommand::FirstRunServiceCompleted(bool success) {
  GetMutableDebugValue().Set("first_run_success", success);
  if (!success) {
    CompleteAndSelfDestruct(CommandResult::kFailure, nullptr, nullptr,
                            apps::LaunchContainer::kLaunchContainerNone);
    return;
  }
}

void LaunchWebAppCommand::OnOsIntegrationSynchronized() {
  GetMutableDebugValue().Set("os_integration_synchronized", true);
}

void LaunchWebAppCommand::DoLaunch() {
  provider_->ui_manager().LaunchWebApp(
      std::move(params_), launch_setting_, *profile_,
      base::BindOnce(&LaunchWebAppCommand::OnAppLaunched,
                     weak_factory_.GetWeakPtr()),
      *lock_);
}

void LaunchWebAppCommand::OnAppLaunched(
    base::WeakPtr<Browser> browser,
    base::WeakPtr<content::WebContents> web_contents,
    apps::LaunchContainer container,
    base::Value debug_value) {
  GetMutableDebugValue().Set("launch_web_app_debug_value",
                             std::move(debug_value));
  CompleteAndSelfDestruct(CommandResult::kSuccess, std::move(browser),
                          std::move(web_contents), container);
}

}  // namespace web_app
