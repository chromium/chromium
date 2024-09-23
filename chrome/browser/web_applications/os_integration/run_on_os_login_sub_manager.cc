
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/run_on_os_login_sub_manager.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_runner.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_run_on_os_login.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/proto/web_app_install_state.pb.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"

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
                                               WebAppProvider& provider)
    : profile_(profile), provider_(provider) {}

RunOnOsLoginSubManager::~RunOnOsLoginSubManager() = default;

void RunOnOsLoginSubManager::Configure(
    const webapps::AppId& app_id,
    proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure configure_done) {
  DCHECK(!desired_state.has_run_on_os_login());

  if (provider_->registrar_unsafe().GetInstallState(app_id) !=
      proto::INSTALLED_WITH_OS_INTEGRATION) {
    std::move(configure_done).Run();
    return;
  }

  proto::RunOnOsLogin* run_on_os_login =
      desired_state.mutable_run_on_os_login();

  const auto login_mode =
      provider_->registrar_unsafe().GetAppRunOnOsLoginMode(app_id);
  run_on_os_login->set_run_on_os_login_mode(
      ConvertWebAppRunOnOsLoginModeToProto(login_mode.value));

  std::move(configure_done).Run();
}

void RunOnOsLoginSubManager::Execute(
    const webapps::AppId& app_id,
    const std::optional<SynchronizeOsOptions>& synchronize_options,
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

  CHECK_OS_INTEGRATION_ALLOWED();

  StartUnregistration(
      app_id, current_state, desired_state,
      base::BindOnce(&RunOnOsLoginSubManager::CreateShortcutInfoWithFavicons,
                     weak_ptr_factory_.GetWeakPtr(), app_id, desired_state,
                     std::move(execute_done)));
}

void RunOnOsLoginSubManager::ForceUnregister(const webapps::AppId& app_id,
                                             base::OnceClosure callback) {
  if (!DoesRunOnOsLoginRequireExecution()) {
    std::move(callback).Run();
    return;
  }

  ResultCallback unregistation_callback =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.RunOnOsLogin.Unregistration.Result",
                                  (result == Result::kOk));
      }).Then(std::move(callback));

  internals::GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &internals::UnregisterRunOnOsLogin, app_id, profile_->GetPath(),
          base::UTF8ToUTF16(
              provider_->registrar_unsafe().GetAppShortName(app_id))),
      std::move(unregistation_callback));
}

void RunOnOsLoginSubManager::StartUnregistration(
    const webapps::AppId& app_id,
    const proto::WebAppOsIntegrationState& current_state,
    const proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure registration_callback) {
  if (!current_state.has_run_on_os_login()) {
    std::move(registration_callback).Run();
    return;
  }

  ResultCallback continue_to_registration =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.RunOnOsLogin.Unregistration.Result",
                                  (result == Result::kOk));
      }).Then(std::move(registration_callback));

  internals::GetShortcutIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &internals::UnregisterRunOnOsLogin, app_id, profile_->GetPath(),
          base::UTF8ToUTF16(
              provider_->registrar_unsafe().GetAppShortName(app_id))),
      std::move(continue_to_registration));
}

void RunOnOsLoginSubManager::CreateShortcutInfoWithFavicons(
    const webapps::AppId& app_id,
    const proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure execute_done) {
  if (!ShouldTriggerRunOnOsLoginRegistration(desired_state)) {
    std::move(execute_done).Run();
    return;
  }

  const WebApp* web_app = provider_->registrar_unsafe().GetAppById(app_id);
  DCHECK(web_app);
  PopulateFaviconForShortcutInfo(
      web_app, provider_->icon_manager(),
      BuildShortcutInfoWithoutFavicon(
          app_id, provider_->registrar_unsafe().GetAppStartUrl(app_id),
          profile_->GetPath(),
          profile_->GetPrefs()->GetString(prefs::kProfileName), desired_state),
      base::BindOnce(
          &RunOnOsLoginSubManager::OnShortcutInfoCreatedStartRegistration,
          weak_ptr_factory_.GetWeakPtr(), app_id, desired_state,
          std::move(execute_done)));
}

void RunOnOsLoginSubManager::OnShortcutInfoCreatedStartRegistration(
    const webapps::AppId& app_id,
    const proto::WebAppOsIntegrationState& desired_state,
    base::OnceClosure execute_done,
    std::unique_ptr<ShortcutInfo> shortcut_info) {
  DCHECK(ShouldTriggerRunOnOsLoginRegistration(desired_state));

  ResultCallback record_metric_and_complete =
      base::BindOnce([](Result result) {
        base::UmaHistogramBoolean("WebApp.RunOnOsLogin.Registration.Result",
                                  (result == Result::kOk));
      }).Then(std::move(execute_done));

  ScheduleRegisterRunOnOsLogin(std::move(shortcut_info),
                               std::move(record_metric_and_complete));
}

}  // namespace web_app
