// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_run_on_os_login_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {
bool g_skip_startup_for_testing_ = false;
}

WebAppRunOnOsLoginManager::WebAppRunOnOsLoginManager(Profile* profile)
    : profile_(profile) {}
WebAppRunOnOsLoginManager::~WebAppRunOnOsLoginManager() {
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

void WebAppRunOnOsLoginManager::SetProvider(base::PassKey<WebAppProvider>,
                                            WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppRunOnOsLoginManager::Start() {
  if (g_skip_startup_for_testing_) {
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin)) {
    return;
  }

  network::mojom::ConnectionType connection_type;
  // `GetConnectionType` will execute either synchronously (and return true and
  // store the value in the `connection_type`) or asynchronously (and return
  // false and call `OnInitialConnectionTypeReceived` once it is done).
  const bool call_was_synchronous =
      content::GetNetworkConnectionTracker()->GetConnectionType(
          &connection_type,
          base::BindOnce(
              &WebAppRunOnOsLoginManager::OnInitialConnectionTypeReceived,
              GetWeakPtr()));
  if (call_was_synchronous) {
    OnInitialConnectionTypeReceived(connection_type);
  }
}

void WebAppRunOnOsLoginManager::RunAppsOnOsLogin(
    AllAppsLock& lock,
    base::Value::Dict& debug_value) {
  base::flat_map<webapps::AppId, WebAppUiManager::RoolNotificationBehavior>
      notification_behaviors;

  for (const webapps::AppId& app_id : lock.registrar().GetAppIds()) {
    if (!IsRunOnOsLoginModeEnabledForAutostart(
            lock.registrar().GetAppRunOnOsLoginMode(app_id).value)) {
      continue;
    }

    WebAppUiManager::RoolNotificationBehavior behavior{
        .is_rool_enabled = true,
        .is_prevent_close_enabled =
            lock.registrar().IsPreventCloseEnabled(app_id)};
    notification_behaviors.insert({app_id, std::move(behavior)});
    debug_value.EnsureList("app_ids")->Append(app_id);

    // In case of already opened/restored apps, we do not launch them again
    if (lock.ui_manager().GetNumWindowsForApp(app_id) > 0) {
      continue;
    }

    // TODO(crbug.com/40698043): Implement Run on OS Login mode selection and
    // launch app appropriately.
    // For ROOL on ChromeOS, we only have managed web apps which need to be run
    // as standalone windows, never as tabs
    apps::AppLaunchParams params(
        app_id, apps::LaunchContainer::kLaunchContainerWindow,
        WindowOpenDisposition::NEW_WINDOW, apps::LaunchSource::kFromOsLogin);

    // Schedule launch here, show notification when the app window pops up.
    provider_->scheduler().LaunchAppWithCustomParams(std::move(params),
                                                     base::DoNothing());
  }

  if (!notification_behaviors.empty()) {
    provider_->ui_manager().DisplayRunOnOsLoginNotification(
        notification_behaviors, profile_->GetWeakPtr());
  }
}

void WebAppRunOnOsLoginManager::OnInitialConnectionTypeReceived(
    network::mojom::ConnectionType type) {
  CHECK(!scheduled_run_on_os_login_command_);

  // If there is a connection, schedule ROOL and stop listening to the network
  // status.
  if (type != network::mojom::ConnectionType::CONNECTION_NONE) {
    RunOsLoginAppsAndMaybeUnregisterObserver();
    return;
  }
  // Otherwise, start listening to the network status and wait until the network
  // connection is restored.
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
}

void WebAppRunOnOsLoginManager::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  CHECK(!scheduled_run_on_os_login_command_);

  // If there is a connection, schedule ROOL and stop listening to the network
  // status. Otherwise, keep listening.
  if (type != network::mojom::ConnectionType::CONNECTION_NONE) {
    RunOsLoginAppsAndMaybeUnregisterObserver();
  }
}

void WebAppRunOnOsLoginManager::RunOsLoginAppsAndMaybeUnregisterObserver() {
  CHECK(!scheduled_run_on_os_login_command_);

  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  scheduled_run_on_os_login_command_ = true;
  provider_->scheduler().ScheduleCallback(
      "WebAppRunOnOsLoginManager::RunAppsOnOsLogin", AllAppsLockDescription(),
      base::BindOnce(&WebAppRunOnOsLoginManager::RunAppsOnOsLogin,
                     GetWeakPtr()),
      /*on_complete=*/std::move(completed_closure_));
}

base::WeakPtr<WebAppRunOnOsLoginManager>
WebAppRunOnOsLoginManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

// static
base::AutoReset<bool> WebAppRunOnOsLoginManager::SkipStartupForTesting() {
  return {&g_skip_startup_for_testing_, true};
}

void WebAppRunOnOsLoginManager::RunAppsOnOsLoginForTesting() {
  provider_->scheduler().ScheduleCallback(
      "WebAppRunOnOsLoginManager::RunAppsOnOsLogin", AllAppsLockDescription(),
      base::BindOnce(&WebAppRunOnOsLoginManager::RunAppsOnOsLogin,
                     GetWeakPtr()),
      /*on_complete=*/std::move(completed_closure_));
}

void WebAppRunOnOsLoginManager::SetCompletedClosureForTesting(
    base::OnceClosure completed_closure) {
  completed_closure_ = std::move(completed_closure);
}

}  // namespace web_app
