// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/screen_ai/public/cpp/screen_ai_chromeos_installer.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "components/services/screen_ai/public/cpp/screen_ai_install_state.h"
#include "components/services/screen_ai/public/cpp/utilities.h"

namespace {

constexpr char kScreenAIDlcName[] = "screen-ai";

void OnInstallCompleted(
    const ash::DlcserviceClient::InstallResult& install_result) {
  // TODO(https://crbug.com/1278249): Add mertics for install result.
  if (install_result.error != dlcservice::kErrorNone) {
    VLOG(0) << "ScreenAI installation failed: " << install_result.error;
    screen_ai::ScreenAIInstallState::GetInstance()->SetState(
        screen_ai::ScreenAIInstallState::State::kFailed);
    return;
  }

  VLOG(0) << "ScreenAI installation completed in path: "
          << install_result.root_path;
  if (!install_result.root_path.empty()) {
    screen_ai::ScreenAIInstallState::GetInstance()->SetComponentFolder(
        base::FilePath(install_result.root_path));
  }
}

void OnUninstallCompleted(const std::string& err) {
  if (err != dlcservice::kErrorNone)
    VLOG(0) << "Unistall failed: " << err;
}

void OnInstallProgress(double progress) {
  screen_ai::ScreenAIInstallState::GetInstance()->SetDownloadProgress(progress);
}

void Install() {
  screen_ai::ScreenAIInstallState::GetInstance()->SetState(
      screen_ai::ScreenAIInstallState::State::kDownloading);

  dlcservice::InstallRequest install_request;
  install_request.set_id(kScreenAIDlcName);
  ash::DlcserviceClient::Get()->Install(
      install_request, base::BindOnce(&OnInstallCompleted),
      base::BindRepeating(&OnInstallProgress));
}

void Uninstall() {
  ash::DlcserviceClient::Get()->Uninstall(
      kScreenAIDlcName, base::BindOnce(&OnUninstallCompleted));
}

}  // namespace

namespace screen_ai::chrome_os_installer {

void ManageInstallation(PrefService* local_state) {
  if (screen_ai::ScreenAIInstallState::ShouldInstall(local_state)) {
    Install();
    return;
  }

  // TODO(https://crbug.com/1278249): Consider adding a timer to keep the DLC
  // for a short time if it is installed and is not needed now.
  Uninstall();
}

}  // namespace screen_ai::chrome_os_installer
