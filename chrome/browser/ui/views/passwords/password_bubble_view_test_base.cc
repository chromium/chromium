// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

class TestManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  explicit TestManagePasswordsUIController(
      content::WebContents* web_contents,
      base::WeakPtr<PasswordsModelDelegate> model_delegate);

  base::WeakPtr<PasswordsModelDelegate> GetModelDelegateProxy() override {
    return model_delegate_;
  }

 private:
  base::WeakPtr<PasswordsModelDelegate> model_delegate_;
};

TestManagePasswordsUIController::TestManagePasswordsUIController(
    content::WebContents* web_contents,
    base::WeakPtr<PasswordsModelDelegate> model_delegate)
    : ManagePasswordsUIController(web_contents),
      model_delegate_(std::move(model_delegate)) {
  DCHECK(model_delegate_);
  // Do not silently replace an existing ManagePasswordsUIController
  // because it unregisters itself in WebContentsDestroyed().
  EXPECT_FALSE(web_contents->GetUserData(UserDataKey()));
  web_contents->SetUserData(UserDataKey(), base::WrapUnique(this));
}

}  // namespace

PasswordBubbleViewTestBase::PasswordBubbleViewTestBase()
    : profile_(IdentityTestEnvironmentProfileAdaptor::
                   CreateProfileForIdentityTestEnvironment({})),
      identity_test_env_profile_adaptor_(
          std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
              profile_.get())),
      test_web_contents_(
          content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                            nullptr)),
      model_delegate_weak_ptr_factory_(&model_delegate_mock_) {
  ON_CALL(model_delegate_mock_, GetWebContents)
      .WillByDefault(Return(web_contents()));
  ON_CALL(model_delegate_mock_, GetPasswordFeatureManager)
      .WillByDefault(Return(feature_manager_mock()));

  // Create the test UIController here so that it's bound to
  // |test_web_contents_|, and will be retrieved correctly via
  // ManagePasswordsUIController::FromWebContents in
  // PasswordsModelDelegateFromWebContents().
  TestManagePasswordsUIController* controller =
      new TestManagePasswordsUIController(
          test_web_contents_.get(),
          model_delegate_weak_ptr_factory_.GetWeakPtr());
  // Set a stub password manager client to avoid a DCHECK failure in
  // |ManagePasswordsState|.
  controller->set_client(&password_manager_client_);
}

PasswordBubbleViewTestBase::~PasswordBubbleViewTestBase() = default;

void PasswordBubbleViewTestBase::CreateAnchorViewAndShow() {
  anchor_widget_ =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       views::Widget::InitParams::TYPE_WINDOW);
  anchor_widget_->Show();
}

void PasswordBubbleViewTestBase::TearDown() {
  anchor_widget_.reset();
  ChromeViewsTestBase::TearDown();
}
