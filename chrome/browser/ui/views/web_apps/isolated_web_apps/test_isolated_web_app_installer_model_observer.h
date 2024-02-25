// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_INSTALLER_MODEL_OBSERVER_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_INSTALLER_MODEL_OBSERVER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/views/web_apps/isolated_web_apps/isolated_web_app_installer_model.h"

namespace web_app {

class TestIsolatedWebAppInstallerModelObserver
    : public IsolatedWebAppInstallerModel::Observer {
 public:
  explicit TestIsolatedWebAppInstallerModelObserver(
      IsolatedWebAppInstallerModel* model);
  ~TestIsolatedWebAppInstallerModelObserver() override;

  void WaitForChildDialog();

  void WaitForStepChange(IsolatedWebAppInstallerModel::Step step);

 private:
  void WaitForStepChange();

  // `IsolatedWebAppInstallerModel::Observer`:
  void OnStepChanged() override;
  void OnChildDialogChanged() override;

  raw_ptr<IsolatedWebAppInstallerModel> model_;
  base::OnceClosure step_changed_callback_;
  base::OnceClosure dialog_changed_callback_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_ISOLATED_WEB_APPS_TEST_ISOLATED_WEB_APP_INSTALLER_MODEL_OBSERVER_H_
