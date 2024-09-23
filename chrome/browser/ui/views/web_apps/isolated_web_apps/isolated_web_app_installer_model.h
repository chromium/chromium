// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_MODEL_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/signed_web_bundle_metadata.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace web_app {

class IsolatedWebAppInstallerModel {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnStepChanged() = 0;
    virtual void OnChildDialogChanged() = 0;
  };

  enum class Step {
    kNone,
    kDisabled,
    kGetMetadata,
    kShowMetadata,
    kInstall,
    kInstallSuccess,
  };

  struct BundleInvalidDialog {};
  struct BundleAlreadyInstalledDialog {
    std::u16string bundle_name;
    base::Version installed_version;
  };
  struct ConfirmInstallationDialog {
    explicit ConfirmInstallationDialog(
        const base::RepeatingClosure& learn_more_callback);
    ConfirmInstallationDialog(const ConfirmInstallationDialog&);
    ConfirmInstallationDialog& operator=(const ConfirmInstallationDialog&);
    ~ConfirmInstallationDialog();

    base::RepeatingClosure learn_more_callback;
  };
  struct InstallationFailedDialog {};

  using Dialog = absl::variant<BundleInvalidDialog,
                               BundleAlreadyInstalledDialog,
                               ConfirmInstallationDialog,
                               InstallationFailedDialog>;

  explicit IsolatedWebAppInstallerModel(const IwaSourceBundleWithMode& source);
  ~IsolatedWebAppInstallerModel();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const IwaSourceBundleWithMode& source() { return source_; }

  void SetStep(Step step);
  Step step() { return step_; }

  void SetSignedWebBundleMetadata(
      const SignedWebBundleMetadata& bundle_metadata);
  const SignedWebBundleMetadata& bundle_metadata() { return *bundle_metadata_; }

  void SetDialog(std::optional<Dialog> dialog);
  bool has_dialog() { return dialog_.has_value(); }
  const Dialog& dialog() { return dialog_.value(); }

 private:
  base::ObserverList<Observer> observers_;
  IwaSourceBundleWithMode source_;
  Step step_;
  std::optional<SignedWebBundleMetadata> bundle_metadata_;
  std::optional<Dialog> dialog_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_INSTALLER_MODEL_H_
