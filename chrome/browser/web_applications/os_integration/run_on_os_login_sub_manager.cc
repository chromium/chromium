
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/run_on_os_login_sub_manager.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_runner.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using content::BrowserThread;

namespace web_app {

namespace {

proto::RunOnOsLoginMode ConvertWebAppRunOnOsLoginModeToProto(
    RunOnOsLoginMode mode) {
  switch (mode) {
    case RunOnOsLoginMode::kMinimized:
      return proto::RunOnOsLoginMode::MINIMIZED;
    case RunOnOsLoginMode::kWindowed:
      return proto::RunOnOsLoginMode::WINDOWED;
    case RunOnOsLoginMode::kNotRun:
      return proto::RunOnOsLoginMode::NOT_RUN;
  }
}

// On non-desktop platforms (like ChromeOS and Android), we do not trigger
// Run on OS Login. ChromeOS does support Run on OS login but its behavior is
// different from other platforms, see web_app_run_on_os_login_manager.h for
// more info.
bool DoesRunOnOsLoginRequireExecution() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
  return base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin);
#else
  return false;
#endif
}

bool ShouldTriggerRunOnOsLoginRegistration(
    const proto::WebAppOsIntegrationState& state) {
  if (!state.has_run_on_os_login()) {
    return false;
  }
  DCHECK(state.run_on_os_login().has_run_on_os_login_mode());
  return (state.run_on_os_login().run_on_os_login_mode() ==
          proto::RunOnOsLoginMode::WINDOWED);
}

}  // namespace

RunOnOsLoginSubManager::RunOnOsLoginSubManager(Profile& profile,
                                               WebAppRegistrar& registrar,
                                               WebAppSyncBridge& sync_bridge,
                                               WebAppIconManager& icon_manager)
    : profile_(profile),
      registrar_(registrar),
      sync_bridge_(sync_bridge),
      icon_manager_(icon_manager) {}

RunOnOsLoginSubManager::~RunOnOsLoginSubManager() = default;

void RunOnOsLoginSubManager::Configure(
    const AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_run_on_os_login());

  if (!registrar_->IsLocallyInstalled(app_id)) {
    std::move(configure_done).Run();
    return;
  }

  proto::RunOnOsLogin* run_on_os_login =
      desired_state.mutable_run_on_os_login();

  const auto login_mode = registrar_->GetAppRunOnOsLoginMode(app_id);
  run_on_os_login->set_run_on_os_login_mode(
      ConvertWebAppRunOnOsLoginModeToProto(login_mode.value));

  std::move(configure_done).Run();
}

void RunOnOsLoginSubManager::Start() {}

void RunOnOsLoginSubManager::Shutdown() {}

void RunOnOsLoginSubManager::Execute(
    const AppId& app_id,
    const absl::optional<SynchronizeOsOptions>& synchronize_options,
    const proto::WebAppOsIntegrationState& desired_state,
    const proto::WebAppOsIntegrationState& current_state,
    base::OnceClosure execute_done) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!DoesRunOnOsLoginRequireExecution()) {
    std::move(execute_done).Run();
    return;
  }

  if (!desired_state.has_run_on_os_login() &&
      !current_state.has_run_on_os_login()) {
    std::move(execute_done).Run();
    return;
  }

  if (desired_state.has_run_on_os_login() &&
      current_state.has_run_on_os_login() &&
      (desired_state.run_on_os_login().SerializeAsString() ==
       current_state.run_on_os_login().SerializeAsString())) {
    std::move(execute_done).Run();
    return;
  }

  StartUnregistration(
      app_id, current_state, desired_state,
      base::BindOnce(&RunOnOsLoginSubManager::CreateShortcutInfoWithFavicons,
                     weak_ptr_factory_.GetWeakPtr(), app_id, desired_state,
                     std::move(execute_done)));
}

void RunOnOsLoginSubManager::StartUnregistration(
    const AppId& app_id,
    const proto::WebAppOsIntegrationState& current_state,
    const proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure registration_callback) {
  if (!current_state.has_run_on_os_login()) {
    std::move(registration_callback).Run();
    return;
  }

  // TODO(crbug.com/1401125): Remove once sub managers have been implemented and
  //  OsIntegrationManager::Synchronize() is running fine.
  if (!desired_state.has_run_on_os_login()) {
    ScopedRegistryUpdate update(&sync_bridge_.get());
    update->UpdateApp(app_id)->SetRunOnOsLoginOsIntegrationState(
        RunOnOsLoginMode::kNotRun);
  }

  ResultCallback continue_to_registration =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.RunOnOsLogin.Unregistration.Result",
                                  (result == Result::kOk));
      }).Then(std::move(registration_callback));

  internals::GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&internals::UnregisterRunOnOsLogin, app_id,
                     profile_->GetPath(),
                     base::UTF8ToUTF16(registrar_->GetAppShortName(app_id))),
      std::move(continue_to_registration));
}

void RunOnOsLoginSubManager::CreateShortcutInfoWithFavicons(
    const AppId& app_id,
    const proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure execute_done) {
  if (!ShouldTriggerRunOnOsLoginRegistration(desired_state)) {
    std::move(execute_done).Run();
    return;
  }

  const WebApp* web_app = registrar_->GetAppById(app_id);
  DCHECK(web_app);
  PopulateFaviconForShortcutInfo(
      web_app, *icon_manager_,
      BuildShortcutInfoWithoutFavicon(
          app_id, registrar_->GetAppStartUrl(app_id), profile_->GetPath(),
          profile_->GetPrefs()->GetString(prefs::kProfileName), desired_state),
      base::BindOnce(
          &RunOnOsLoginSubManager::OnShortcutInfoCreatedStartRegistration,
          weak_ptr_factory_.GetWeakPtr(), app_id, desired_state,
          std::move(execute_done)));
}

void RunOnOsLoginSubManager::OnShortcutInfoCreatedStartRegistration(
    const AppId& app_id,
    const proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure execute_done,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  DCHECK(ShouldTriggerRunOnOsLoginRegistration(desired_state));
  // TODO(crbug.com/1401125): Remove once sub managers have been implemented and
  //  OsIntegrationManager::Synchronize() is running fine.
  {
    ScopedRegistryUpdate update(&sync_bridge_.get());
    update->UpdateApp(app_id)->SetRunOnOsLoginOsIntegrationState(
        RunOnOsLoginMode::kWindowed);
  }

  ResultCallback record_metric_and_complete =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.RunOnOsLogin.Registration.Result",
                                  (result == Result::kOk));
      }).Then(std::move(execute_done));

  ScheduleRegisterRunOnOsLogin(&sync_bridge_.get(), std::move(shortcut_info),
                               std::move(record_metric_and_complete));
}

}  // namespace web_app
