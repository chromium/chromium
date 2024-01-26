// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_coordinator.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_view_controller.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/pref_observer.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {
namespace {

void DeleteCoordinatorHelper(
    std::unique_ptr<IsolatedWebAppInstallerCoordinator> coordinator) {
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(coordinator));
}

}  // namespace

IsolatedWebAppInstallerCoordinator* LaunchIsolatedWebAppInstaller(
    Profile* profile,
    const base::FilePath& bundle_path,
    base::OnceClosure on_closed_callback) {
  auto coordinator = std::make_unique<IsolatedWebAppInstallerCoordinator>(
      profile, bundle_path, std::move(on_closed_callback));
  IsolatedWebAppInstallerCoordinator* raw_coordinator = coordinator.get();
  base::OnceClosure delete_callback =
      base::BindOnce(&DeleteCoordinatorHelper, std::move(coordinator));
  raw_coordinator->Show(base::IgnoreArgs<std::optional<webapps::AppId>>(
      std::move(delete_callback)));
  return raw_coordinator;
}

void FocusIsolatedWebAppInstaller(
    IsolatedWebAppInstallerCoordinator* coordinator) {
  coordinator->FocusWindow();
}

IsolatedWebAppInstallerCoordinator::IsolatedWebAppInstallerCoordinator(
    Profile* profile,
    const base::FilePath& bundle_path,
    base::OnceClosure on_closed_callback)
    : on_closed_callback_(std::move(on_closed_callback)),
      model_(std::make_unique<IsolatedWebAppInstallerModel>(bundle_path)),
      controller_(std::make_unique<IsolatedWebAppInstallerViewController>(
          profile,
          WebAppProvider::GetForWebApps(profile),
          model_.get(),
          IsolatedWebAppsEnabledPrefObserver::Create(profile))) {}

IsolatedWebAppInstallerCoordinator::~IsolatedWebAppInstallerCoordinator() =
    default;

void IsolatedWebAppInstallerCoordinator::Show(
    base::OnceCallback<void(std::optional<webapps::AppId>)> callback) {
  controller_->Start(
      base::BindOnce(&IsolatedWebAppInstallerViewController::Show,
                     base::Unretained(controller_.get())),
      base::BindOnce(&IsolatedWebAppInstallerCoordinator::OnDialogClosed,
                     base::Unretained(this), std::move(callback)));
}

void IsolatedWebAppInstallerCoordinator::FocusWindow() {
  controller_->FocusWindow();
}

void IsolatedWebAppInstallerCoordinator::OnDialogClosed(
    base::OnceCallback<void(std::optional<webapps::AppId>)> callback) {
  // Notify installer closed.
  std::move(on_closed_callback_).Run();

  if (model_->step() == IsolatedWebAppInstallerModel::Step::kInstallSuccess) {
    std::move(callback).Run(model_->bundle_metadata().app_id());
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

}  // namespace web_app
