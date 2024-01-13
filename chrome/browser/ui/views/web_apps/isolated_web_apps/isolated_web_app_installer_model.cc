// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"

#include <optional>

#include "base/files/file_path.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"

namespace web_app {

IsolatedWebAppInstallerModel::ConfirmInstallationDialog::
    ConfirmInstallationDialog(const base::RepeatingClosure& learn_more_callback)
    : learn_more_callback(learn_more_callback) {}
IsolatedWebAppInstallerModel::ConfirmInstallationDialog::
    ConfirmInstallationDialog(const ConfirmInstallationDialog&) = default;
IsolatedWebAppInstallerModel::ConfirmInstallationDialog&
IsolatedWebAppInstallerModel::ConfirmInstallationDialog::operator=(
    const IsolatedWebAppInstallerModel::ConfirmInstallationDialog&) = default;
IsolatedWebAppInstallerModel::ConfirmInstallationDialog::
    ~ConfirmInstallationDialog() = default;

IsolatedWebAppInstallerModel::IsolatedWebAppInstallerModel(
    const base::FilePath& bundle_path)
    : bundle_path_(bundle_path), step_(Step::kDisabled) {}

IsolatedWebAppInstallerModel::~IsolatedWebAppInstallerModel() = default;

void IsolatedWebAppInstallerModel::SetStep(Step step) {
  step_ = step;
}

void IsolatedWebAppInstallerModel::SetSignedWebBundleMetadata(
    const SignedWebBundleMetadata& bundle_metadata) {
  bundle_metadata_ = bundle_metadata;
}

void IsolatedWebAppInstallerModel::SetDialog(std::optional<Dialog> dialog) {
  dialog_ = dialog;
}

}  // namespace web_app
