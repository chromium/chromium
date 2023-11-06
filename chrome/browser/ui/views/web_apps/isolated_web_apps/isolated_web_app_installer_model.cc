// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

IsolatedWebAppInstallerModel::DialogContent::DialogContent(
    bool is_error,
    int message,
    int details,
    absl::optional<LinkInfo> details_link,
    absl::optional<int> accept_message)
    : is_error(is_error),
      message(message),
      details(details),
      details_link(details_link),
      accept_message(accept_message) {}

IsolatedWebAppInstallerModel::DialogContent::DialogContent(
    const IsolatedWebAppInstallerModel::DialogContent&) = default;

IsolatedWebAppInstallerModel::DialogContent::~DialogContent() = default;

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

void IsolatedWebAppInstallerModel::SetDialogContent(
    absl::optional<DialogContent> dialog_content) {
  dialog_content_ = dialog_content;
}

}  // namespace web_app
