// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_MODEL_H_

#include <string>

#include "base/files/file_path.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

class IsolatedWebAppInstallerModel {
 public:
  enum class Step {
    kDisabled,
    kGetMetadata,
    kConfirmInstall,
    kInstall,
    kInstallSuccess,
  };

  struct DialogContent {
    bool is_error = true;
    bool is_recoverable = false;
    int message_id = -1;
    std::u16string details;
  };

  explicit IsolatedWebAppInstallerModel(const base::FilePath& bundle_path);
  ~IsolatedWebAppInstallerModel();

  const base::FilePath& bundle_path() { return bundle_path_; }

  void SetStep(Step step);
  Step step() { return step_; }

  void SetSignedWebBundleMetadata(
      const SignedWebBundleMetadata& bundle_metadata);
  const SignedWebBundleMetadata& bundle_metadata() { return *bundle_metadata_; }

  void SetDialogContent(absl::optional<DialogContent> dialog_content);
  absl::optional<DialogContent> dialog_content() { return dialog_content_; }

 private:
  base::FilePath bundle_path_;
  Step step_;
  absl::optional<SignedWebBundleMetadata> bundle_metadata_;
  absl::optional<DialogContent> dialog_content_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_MODEL_H_
