// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_update_apps.h"

#include <stdio.h>

#include <iomanip>
#include <iostream>
#include <tuple>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/lock.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace updater {

namespace {

// TODO(crbug.com/440373986): internationalize the text.
void OnAppStateChanged(const UpdateService::UpdateState& update_state) {
  switch (update_state.state) {
    case UpdateService::UpdateState::State::kCheckingForUpdates:
      std::cout << std::quoted(update_state.app_id)
                << ": checking for updates..." << std::endl;
      break;

    case UpdateService::UpdateState::State::kUpdateAvailable:
      std::cout << std::quoted(update_state.app_id)
                << ": update available, version: " << update_state.next_version
                << std::endl;
      break;

    case UpdateService::UpdateState::State::kDownloading:
      std::cout << std::quoted(update_state.app_id)
                << ": downloading update, downloaded bytes: "
                << update_state.downloaded_bytes
                << ", total bytes: " << update_state.total_bytes << std::endl;
      break;

    case UpdateService::UpdateState::State::kDecompressing:
      std::cout << std::quoted(update_state.app_id)
                << ": decompressing update..." << std::endl;
      break;

    case UpdateService::UpdateState::State::kPatching:
      std::cout << std::quoted(update_state.app_id) << ": applying patches..."
                << std::endl;
      break;

    case UpdateService::UpdateState::State::kInstalling:
      std::cout << std::quoted(update_state.app_id)
                << ": installing update: " << update_state.install_progress
                << std::endl;
      break;

    case UpdateService::UpdateState::State::kUpdated:
      std::cout << std::quoted(update_state.app_id)
                << ": updated version: " << update_state.next_version
                << std::endl;
      break;

    case UpdateService::UpdateState::State::kNoUpdate:
      std::cout << std::quoted(update_state.app_id) << ": up-to-date."
                << std::endl;
      break;

    case UpdateService::UpdateState::State::kUpdateError:
      std::cout << std::quoted(update_state.app_id) << ": update failed"
                << ", error code: " << update_state.error_code
                << ", extra code: " << update_state.extra_code1 << std::endl;
      break;

    default:
      std::cout << std::quoted(update_state.app_id)
                << ": update state: " << update_state.state << std::endl;
      break;
  }
}

void OnUpdateComplete(const std::string& app_id,
                      base::OnceClosure cb,
                      UpdateService::Result result) {
  if (result == UpdateService::Result::kSuccess) {
    std::cout << std::quoted(app_id) << ": update completed successfully"
              << std::endl;
  } else {
    std::cout << std::quoted(app_id) << ": update failed, result = " << result
              << std::endl;
  }
  std::move(cb).Run();
}

}  // namespace

class AppUpdateApps : public App {
 private:
  ~AppUpdateApps() override = default;
  [[nodiscard]] int Initialize() override;
  void FirstTaskRun() override;
  void DoUpdateApps(
      const std::vector<updater::UpdateService::AppState>& states);

  scoped_refptr<UpdateService> service_proxy_;
};

int AppUpdateApps::Initialize() {
#if BUILDFLAG(IS_WIN)
  if (!::AttachConsole(ATTACH_PARENT_PROCESS) && !::AllocConsole()) {
    return ::GetLastError();
  }
  for (const auto [filename, mode, stream] :
       std::vector<std::tuple<const char*, const char*, FILE*>>{
           {"CONIN$", "r", stdin},
           {"CONOUT$", "w", stdout},
           {"CONOUT$", "w", stderr}}) {
    FILE* file;
    const errno_t err = freopen_s(&file, filename, mode, stream);
    if (err) {
      return err;
    }
  }
#endif

  return kErrorOk;
}

void AppUpdateApps::DoUpdateApps(
    const std::vector<updater::UpdateService::AppState>& states) {
  if (states.empty()) {
    std::cout << "Done running `--update-apps`" << std::endl;
    Shutdown(kErrorOk);
    return;
  }

  const std::string app_id = states.front().app_id;
  service_proxy_->Update(
      app_id, /*install_data_index=*/"", UpdateService::Priority::kForeground,
      UpdateService::PolicySameVersionUpdate::kNotAllowed,
      /*language=*/{}, base::BindRepeating(OnAppStateChanged),
      base::BindOnce(
          [](const std::string& app_id, base::OnceClosure cb,
             UpdateService::Result result) {
            OnUpdateComplete(app_id, std::move(cb), result);
          },
          app_id,
          base::BindOnce(&AppUpdateApps::DoUpdateApps, this,
                         std::vector<updater::UpdateService::AppState>(
                             states.begin() + 1, states.end()))));
}

void AppUpdateApps::FirstTaskRun() {
  if (!IsSystemInstall(updater_scope()) && WrongUser(updater_scope())) {
    std::cout << "The current user is not compatible with the current scope."
              << std::endl;
    Shutdown(kErrorWrongUser);
    return;
  }

  service_proxy_ = CreateUpdateServiceProxy(updater_scope());
  service_proxy_->GetAppStates(
      base::BindOnce(&AppUpdateApps::DoUpdateApps, this));
}

scoped_refptr<App> MakeAppUpdateApps() {
  return base::MakeRefCounted<AppUpdateApps>();
}

}  // namespace updater
