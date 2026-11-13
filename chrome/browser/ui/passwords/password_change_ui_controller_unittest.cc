// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_change_ui_controller.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/passwords/password_change_ui_delegate_mock.h"
#include "chrome/browser/ui/views/passwords/password_change/password_change_toast.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/layout/layout_provider.h"

namespace {

using password_manager::CreateLeakType;
using password_manager::HasChangePasswordUrl;
using password_manager::IsReused;
using password_manager::IsSaved;
using password_manager::IsSyncing;
using password_manager::LeakedPasswordDetails;
using ::testing::NiceMock;
using ::testing::Return;

class MockManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  MockManagePasswordsUIController(tabs::TabInterface& tab,
                                  content::WebContents* web_contents)
      : ManagePasswordsUIController(tab, web_contents) {}
  ~MockManagePasswordsUIController() override = default;

  MOCK_METHOD(void, OnPasswordChangeFinishedSuccessfully, (), (override));
  MOCK_METHOD(void, OnLeakDialogHidden, (), (override));
  MOCK_METHOD(void, OnCredentialLeak, (LeakedPasswordDetails), (override));
  void PrimaryPageChanged(content::Page& page) override {}
};

LeakedPasswordDetails CreateLeakedPasswordDetails(
    HasChangePasswordUrl has_change_password_url) {
  password_manager::PasswordForm form;
  form.url = GURL("https://example.com/");
  form.username_value = u"username";
  return LeakedPasswordDetails(
      CreateLeakType(IsSaved(true), IsReused(false), IsSyncing(true),
                     has_change_password_url),
      std::move(form), /*in_account_store=*/true);
}

class TestPasswordChangeUIController : public PasswordChangeUIController {
 public:
  using PasswordChangeUIController::PasswordChangeUIController;

  PasswordChangeToast::ToastOptions& last_toast_options() {
    return *last_toast_options_;
  }

  void set_on_show_toast(base::OnceClosure on_show_toast) {
    on_show_toast_ = std::move(on_show_toast);
  }

 private:
  void ShowToast(PasswordChangeToast::ToastOptions options) override {
    last_toast_options_.emplace(std::move(options));
    if (on_show_toast_) {
      std::move(on_show_toast_).Run();
    }
  }

  std::optional<PasswordChangeToast::ToastOptions> last_toast_options_;
  base::OnceClosure on_show_toast_;
};

class PasswordChangeUIControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ON_CALL(tab_interface_, GetContents).WillByDefault(Return(web_contents()));
    ON_CALL(tab_interface_, GetWeakPtr)
        .WillByDefault(Return(tab_weak_ptr_factory_.GetWeakPtr()));
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &tab_interface_);
    passwords_ui_controller_ =
        std::make_unique<NiceMock<MockManagePasswordsUIController>>(
            tab_interface_, web_contents());
    controller_ = std::make_unique<TestPasswordChangeUIController>(
        &delegate_, &tab_interface_);
  }

  void TearDown() override {
    controller_.reset();
    passwords_ui_controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  // Needed to create dialogs.
  views::LayoutProvider layout_provider_;
  PasswordChangeUIDelegateMock delegate_;
  tabs::MockTabInterface tab_interface_;
  base::WeakPtrFactory<tabs::TabInterface> tab_weak_ptr_factory_{
      &tab_interface_};
  std::unique_ptr<NiceMock<MockManagePasswordsUIController>>
      passwords_ui_controller_;
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

TEST_F(PasswordChangeUIControllerTest,
       SuccessNotifiesManagePasswordsUIController) {
  EXPECT_CALL(*passwords_ui_controller_, OnPasswordChangeFinishedSuccessfully);
  controller_->UpdateState(
      PasswordChangeUIDelegate::State::kPasswordSuccessfullyChanged);
}

TEST_F(PasswordChangeUIControllerTest,
       NonSuccessStateDoesNotNotifyManagePasswordsUIController) {
  EXPECT_CALL(*passwords_ui_controller_, OnPasswordChangeFinishedSuccessfully)
      .Times(0);
  controller_->UpdateState(PasswordChangeUIDelegate::State::kCanceled);
}

TEST_F(PasswordChangeUIControllerTest,
       DeclineNotifiesManagePasswordsUIControllerAsynchronously) {
  controller_->UpdateState(
      PasswordChangeUIDelegate::State::kOfferingPasswordChange);

  EXPECT_CALL(delegate_, OnPasswordChangeDeclined);
  EXPECT_CALL(*passwords_ui_controller_, OnLeakDialogHidden).Times(0);
  controller_->CallOnDialogCanceledForTesting();
  testing::Mock::VerifyAndClearExpectations(passwords_ui_controller_.get());

  EXPECT_CALL(*passwords_ui_controller_, OnLeakDialogHidden);
  task_environment()->RunUntilIdle();
}

TEST_F(PasswordChangeUIControllerTest,
       DismissingNonOfferDialogDoesNotNotifyLeakDialogHidden) {
  controller_->UpdateState(
      PasswordChangeUIDelegate::State::kPasswordChangeFailed);

  EXPECT_CALL(delegate_, OnUiDismissed);
  EXPECT_CALL(*passwords_ui_controller_, OnLeakDialogHidden).Times(0);
  controller_->CallOnDialogCanceledForTesting();
  task_environment()->RunUntilIdle();
}

TEST_F(PasswordChangeUIControllerTest,
       LoginCheckServerErrorShowsLeakWarningWithoutChangePasswordUrl) {
  EXPECT_CALL(*passwords_ui_controller_,
              OnCredentialLeak(
                  CreateLeakedPasswordDetails(HasChangePasswordUrl(false))));
  controller_->OnLoginCheckFailedWithServerError(
      CreateLeakedPasswordDetails(HasChangePasswordUrl(true)));
}

TEST_F(PasswordChangeUIControllerTest,
       UpdateStateSurvivesSynchronousDestruction) {
  controller_->set_on_show_toast(base::BindOnce(
      [](std::unique_ptr<TestPasswordChangeUIController>* controller) {
        controller->reset();
      },
      base::Unretained(&controller_)));

  controller_->UpdateState(
      PasswordChangeUIDelegate::State::kWaitingForChangePasswordForm);
  EXPECT_EQ(controller_, nullptr);
}

}  // namespace
