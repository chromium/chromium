// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_group_deletion_dialog_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/test/test_dialog_model_host.h"

using DeletionDialogController = tab_groups::DeletionDialogController;

class DeletionDialogControllerUnitTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_ = profile_builder.Build();
    tab_strip_model_delegate_ = std::make_unique<TestTabStripModelDelegate>();
    tab_strip_model_ = std::make_unique<TabStripModel>(
        tab_strip_model_delegate_.get(), profile_.get());
    browser_window_interface_ = std::make_unique<MockBrowserWindowInterface>();

    ON_CALL(*browser_window_interface_, GetTabStripModel())
        .WillByDefault(::testing::Return(tab_strip_model_.get()));
    ON_CALL(*browser_window_interface_, GetProfile())
        .WillByDefault(::testing::Return(profile_.get()));

    controller_ = std::make_unique<DeletionDialogController>(
        browser_window_interface_.get(), profile_.get(), tab_strip_model_.get(),
        base::BindRepeating(&DeletionDialogControllerUnitTest::ShowDialogFn,
                            base::Unretained(this)));
  }

  void ShowDialogFn(std::unique_ptr<ui::DialogModel> dialog_model) {
    dialog_host_ =
        std::make_unique<ui::TestDialogModelHost>(std::move(dialog_model));
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<MockBrowserWindowInterface> browser_window_interface_;
  std::unique_ptr<TestTabStripModelDelegate> tab_strip_model_delegate_;
  std::unique_ptr<TabStripModel> tab_strip_model_;
  std::unique_ptr<ui::TestDialogModelHost> dialog_host_;
  std::unique_ptr<DeletionDialogController> controller_;
};

TEST_F(DeletionDialogControllerUnitTest, OnWidgetDestroyedDestroysState) {
  controller_->MaybeShowDialog(
      DeletionDialogController::DialogMetadata(
          DeletionDialogController::DialogType::DeleteSingle),
      base::DoNothing());
  EXPECT_TRUE(controller_->IsShowingDialog());

  // Force the host to kill the DialogModel.
  ui::TestDialogModelHost::DestroyWithoutAction(std::move(dialog_host_));

  // Make sure the DeletionDialogController has destroyed it's state.
  EXPECT_FALSE(controller_->IsShowingDialog());
}
