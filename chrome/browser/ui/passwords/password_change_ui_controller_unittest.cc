// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_change_ui_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "chrome/browser/ui/passwords/password_change_ui_delegate_mock.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_toast.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Return;

class TestPasswordChangeUIController : public PasswordChangeUIController {
 public:
  using PasswordChangeUIController::PasswordChangeUIController;

  PasswordChangeToast::ToastOptions& last_toast_options() {
    return *last_toast_options_;
  }

 private:
  void ShowToast(PasswordChangeToast::ToastOptions options) override {
    last_toast_options_.emplace(std::move(options));
  }

  std::optional<PasswordChangeToast::ToastOptions> last_toast_options_;
};

class PasswordChangeUIControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ON_CALL(tab_interface_, GetContents).WillByDefault(Return(web_contents()));
    controller_ = std::make_unique<TestPasswordChangeUIController>(
        &delegate_, &tab_interface_);
  }

  void TearDown() override {
    controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  PasswordChangeUIDelegateMock delegate_;
  tabs::MockTabInterface tab_interface_;
  std::unique_ptr<TestPasswordChangeUIController> controller_;
};

TEST_F(PasswordChangeUIControllerTest,
       TerminalToastConfiguresAutoDismiss_OnCanceled) {
  controller_->UpdateState(PasswordChangeUIDelegate::State::kCanceled);

  ASSERT_FALSE(
      controller_->last_toast_options().auto_dismiss_closure.is_null());
  EXPECT_CALL(delegate_, OnUiDismissed);
  std::move(controller_->last_toast_options().auto_dismiss_closure).Run();
}

TEST_F(PasswordChangeUIControllerTest,
       TerminalToastConfiguresAutoDismiss_OnSuccess) {
  controller_->UpdateState(
      PasswordChangeUIDelegate::State::kPasswordSuccessfullyChanged);

  ASSERT_FALSE(
      controller_->last_toast_options().auto_dismiss_closure.is_null());
  EXPECT_CALL(delegate_, OnUiDismissed);
  std::move(controller_->last_toast_options().auto_dismiss_closure).Run();
}

TEST_F(PasswordChangeUIControllerTest,
       NonTerminalToastDoesNotConfigureAutoDismiss) {
  controller_->UpdateState(
      PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm);

  EXPECT_TRUE(controller_->last_toast_options().auto_dismiss_closure.is_null());
}

}  // namespace
