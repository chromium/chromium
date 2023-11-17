// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_MODEL_H_

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

class IsolatedWebAppInstallerModel {
 public:
  enum class Step {
    kDisabled,
    kGetMetadata,
    kShowMetadata,
    kInstall,
    kInstallSuccess,
  };

  using LinkInfo = std::pair<int, base::RepeatingClosure>;
  struct DialogContent {
    DialogContent(bool is_error,
                  int message,
                  int details,
                  absl::optional<LinkInfo> details_link = absl::nullopt,
                  absl::optional<int> accept_message = absl::nullopt);
    DialogContent(const DialogContent&);
    ~DialogContent();

    bool is_error;
    int message;
    int details;
    absl::optional<LinkInfo> details_link;
    // Message on the non-Cancel button of the dialog, if it should be present.
    absl::optional<int> accept_message;
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
  bool has_dialog_content() { return dialog_content_.has_value(); }
  const DialogContent& dialog_content() { return dialog_content_.value(); }

 private:
  base::FilePath bundle_path_;
  Step step_;
  absl::optional<SignedWebBundleMetadata> bundle_metadata_;
  absl::optional<DialogContent> dialog_content_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_MODEL_H_
