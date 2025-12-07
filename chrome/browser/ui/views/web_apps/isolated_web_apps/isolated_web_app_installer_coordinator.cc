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
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
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
  return IsolatedWebAppInstallerCoordinator::CreateAndStart(
      profile, bundle_path, std::move(on_closed_callback),
      IsolatedWebAppsEnabledPrefObserver::Create(profile));
}

// static
IsolatedWebAppInstallerCoordinator*
IsolatedWebAppInstallerCoordinator::CreateAndStart(
    Profile* profile,
    const base::FilePath& bundle_path,
    base::OnceClosure on_closed_callback,
    std::unique_ptr<IsolatedWebAppsEnabledPrefObserver> pref_observer) {
  auto coordinator = base::WrapUnique(new IsolatedWebAppInstallerCoordinator(
      profile, bundle_path, std::move(on_closed_callback),
      std::move(pref_observer)));
  IsolatedWebAppInstallerCoordinator* raw_coordinator = coordinator.get();
  base::OnceClosure delete_callback =
      base::BindOnce(&DeleteCoordinatorHelper, std::move(coordinator));
  raw_coordinator->Start(base::IgnoreArgs<std::optional<webapps::AppId>>(
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
    base::OnceClosure on_closed_callback,
    std::unique_ptr<IsolatedWebAppsEnabledPrefObserver> pref_observer)
    : profile_(profile),
      on_closed_callback_(std::move(on_closed_callback)),
      model_(std::make_unique<IsolatedWebAppInstallerModel>(
          IwaSourceBundleDevMode(bundle_path))),
      controller_(std::make_unique<IsolatedWebAppInstallerViewController>(
          profile,
          WebAppProvider::GetForWebApps(profile),
          model_.get(),
          std::move(pref_observer))) {}

IsolatedWebAppInstallerCoordinator::~IsolatedWebAppInstallerCoordinator() =
    default;

void IsolatedWebAppInstallerCoordinator::Start(
    base::OnceCallback<void(std::optional<webapps::AppId>)> callback) {
  // base::Unretained is safe here because `callback` owns `this`.
  base::OnceClosure on_complete_callback =
      base::BindOnce(&IsolatedWebAppInstallerCoordinator::OnDialogClosed,
                     base::Unretained(this), std::move(callback));
  if (!IsIwaUnmanagedInstallEnabled(profile_)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_complete_callback));
    return;
  }
  controller_->Start(
      base::BindOnce(&IsolatedWebAppInstallerViewController::Show,
                     base::Unretained(controller_.get())),
      std::move(on_complete_callback));
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

IsolatedWebAppInstallerModel*
IsolatedWebAppInstallerCoordinator::GetModelForTesting() {
  return model_.get();
}

IsolatedWebAppInstallerViewController*
IsolatedWebAppInstallerCoordinator::GetControllerForTesting() {
  return controller_.get();
}

}  // namespace web_app
