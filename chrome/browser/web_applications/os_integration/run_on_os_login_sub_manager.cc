
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/run_on_os_login_sub_manager.h"

#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/web_applications/os_integration/os_integration_sub_manager.h"
#include "chrome/browser/web_applications/proto/web_app_os_integration_state.pb.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

}  // namespace

RunOnOsLoginSubManager::RunOnOsLoginSubManager(WebAppRegistrar& registrar)
    : registrar_(registrar) {}

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
    base::OnceClosure callback) {
  // Not implemented yet.
  std::move(callback).Run();
}

}  // namespace web_app
