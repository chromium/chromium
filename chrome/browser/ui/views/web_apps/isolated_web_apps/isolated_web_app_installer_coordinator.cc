// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_coordinator.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

void LaunchIsolatedWebAppInstaller(Profile* profile,
                                   const base::FilePath& bundle_path) {
  auto coordinator = std::make_unique<IsolatedWebAppInstallerCoordinator>(
      profile, bundle_path);
  IsolatedWebAppInstallerCoordinator* raw_coordinator = coordinator.get();
  base::OnceClosure delete_callback =
      base::DoNothingWithBoundArgs(std::move(coordinator));
  raw_coordinator->Show(base::IgnoreArgs<absl::optional<webapps::AppId>>(
      std::move(delete_callback)));
}

IsolatedWebAppInstallerCoordinator::IsolatedWebAppInstallerCoordinator(
    Profile* profile,
    const base::FilePath& bundle_path)
    : model_(std::make_unique<IsolatedWebAppInstallerModel>(bundle_path)),
      controller_(std::make_unique<IsolatedWebAppInstallerViewController>(
          profile,
          WebAppProvider::GetForWebApps(profile),
          model_.get())) {}

IsolatedWebAppInstallerCoordinator::~IsolatedWebAppInstallerCoordinator() =
    default;

void IsolatedWebAppInstallerCoordinator::Show(
    base::OnceCallback<void(absl::optional<webapps::AppId>)> callback) {
  controller_->Start();
  controller_->Show(
      base::BindOnce(&IsolatedWebAppInstallerCoordinator::OnDialogClosed,
                     base::Unretained(this), std::move(callback)));
}

void IsolatedWebAppInstallerCoordinator::OnDialogClosed(
    base::OnceCallback<void(absl::optional<webapps::AppId>)> callback) {
  if (model_->step() == IsolatedWebAppInstallerModel::Step::kInstallSuccess) {
    std::move(callback).Run(model_->bundle_metadata().app_id());
  } else {
    std::move(callback).Run(absl::nullopt);
  }
}

}  // namespace web_app
