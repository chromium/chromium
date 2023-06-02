// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_view.h"

#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_ui.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace {

password_manager::PasswordForm CreateTestPasswordForm(int index = 0) {
  password_manager::PasswordForm form;
  form.url = GURL("https://test" + base::NumberToString(index) + ".com");
  form.signon_realm = form.url.spec();
  form.username_value = u"username" + base::NumberToString16(index);
  form.password_value = u"password" + base::NumberToString16(index);
  return form;
}

}  // namespace

class ManagePasswordsViewTest : public PasswordBubbleViewTestBase {
 public:
  ManagePasswordsViewTest();
  ~ManagePasswordsViewTest() override = default;

  void CreateViewAndShow();

  void TearDown() override {
    view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);

    PasswordBubbleViewTestBase::TearDown();
  }

  ManagePasswordsView* view() { return view_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<ManagePasswordsView, DanglingUntriaged> view_;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms_;
};

ManagePasswordsViewTest::ManagePasswordsViewTest() {
  feature_list_.InitAndEnableFeature(
      password_manager::features::kRevampedPasswordManagementBubble);
  ON_CALL(*model_delegate_mock(), GetOrigin)
      .WillByDefault(Return(url::Origin::Create(CreateTestPasswordForm().url)));
  ON_CALL(*model_delegate_mock(), GetState)
      .WillByDefault(Return(password_manager::ui::MANAGE_STATE));
  ON_CALL(*model_delegate_mock(), GetCurrentForms)
      .WillByDefault(ReturnRef(current_forms_));
}

void ManagePasswordsViewTest::CreateViewAndShow() {
  CreateAnchorViewAndShow();

  view_ = new ManagePasswordsView(web_contents(), anchor_view());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

TEST_F(ManagePasswordsViewTest, HasTitle) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowWindowTitle());
}
