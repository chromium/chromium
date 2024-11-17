// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/test/test_dialog_model_host.h"

class DeletionDialogControllerUnitTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
    controller_ = std::make_unique<tab_groups::DeletionDialogController>(
        profile_.get(),
        base::BindRepeating(&DeletionDialogControllerUnitTest::ShowDialogFn,
                            base::Unretained(this)));
  }

  void ShowDialogFn(std::unique_ptr<ui::DialogModel> dialog_model) {
    dialog_host_ =
        std::make_unique<ui::TestDialogModelHost>(std::move(dialog_model));
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ui::TestDialogModelHost> dialog_host_;
  std::unique_ptr<tab_groups::DeletionDialogController> controller_;
};

TEST_F(DeletionDialogControllerUnitTest, OnWidgetDestroyedDestroysState) {
  controller_->MaybeShowDialog(
      tab_groups::DeletionDialogController::DialogType::DeleteSingle,
      base::DoNothing(), 1, 1);
  EXPECT_TRUE(controller_->IsShowingDialog());

  // Force the host to kill the DialogModel.
  ui::TestDialogModelHost::DestroyWithoutAction(std::move(dialog_host_));

  // Make sure the DeletionDialogController has destroyed it's state.
  EXPECT_FALSE(controller_->IsShowingDialog());
}
