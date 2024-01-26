// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/isolated_web_apps/test_isolated_web_app_installer_model_observer.h"

#include "base/test/test_future.h"

namespace web_app {

TestIsolatedWebAppInstallerModelObserver::
    TestIsolatedWebAppInstallerModelObserver(
        IsolatedWebAppInstallerModel* model)
    : model_(model) {
  model_->AddObserver(this);
}

TestIsolatedWebAppInstallerModelObserver::
    ~TestIsolatedWebAppInstallerModelObserver() {
  model_->RemoveObserver(this);
}

void TestIsolatedWebAppInstallerModelObserver::WaitForChildDialog() {
  CHECK(!dialog_changed_callback_);
  while (!model_->has_dialog()) {
    base::test::TestFuture<void> future;
    dialog_changed_callback_ = future.GetCallback();
    CHECK(future.Wait());
  }
}

void TestIsolatedWebAppInstallerModelObserver::WaitForStepChange(
    IsolatedWebAppInstallerModel::Step step) {
  while (model_->step() != step) {
    WaitForStepChange();
  }
}

void TestIsolatedWebAppInstallerModelObserver::WaitForStepChange() {
  CHECK(!step_changed_callback_);
  base::test::TestFuture<void> future;
  step_changed_callback_ = future.GetCallback();
  CHECK(future.Wait());
}

void TestIsolatedWebAppInstallerModelObserver::OnStepChanged() {
  if (step_changed_callback_) {
    std::move(step_changed_callback_).Run();
  }
}

void TestIsolatedWebAppInstallerModelObserver::OnChildDialogChanged() {
  if (dialog_changed_callback_) {
    std::move(dialog_changed_callback_).Run();
  }
}

}  // namespace web_app
