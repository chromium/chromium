// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_view.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_details_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_view_ids.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/common/password_manager_constants.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"

using ::testing::Mock;
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
    ClearView();
    if (view_widget_) {
      view_widget_->CloseWithReason(
          views::Widget::ClosedReason::kCloseButtonClicked);
    }
    PasswordBubbleViewTestBase::TearDown();
  }

  ManagePasswordsView* view() { return view_; }

  void ClearView() { view_ = nullptr; }

  bool PasswordDetailsHasBackButton() {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForWidget(view_widget_.get());
    return views::ElementTrackerViews::GetInstance()->GetUniqueView(
               ManagePasswordsDetailsView::kBackButton, context) != nullptr;
  }

  bool PasswordDetailsHasEditUsernameButton() {
    return view()->GetViewByID(
               static_cast<int>(password_manager::ManagePasswordsViewIDs::
                                    kEditUsernameButton)) != nullptr;
  }

 private:
  raw_ptr<ManagePasswordsView> view_ = nullptr;
  base::WeakPtr<views::Widget> view_widget_ = nullptr;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms_;
  std::optional<password_manager::PasswordForm>
      single_credential_details_mode_credential_;
};

ManagePasswordsViewTest::ManagePasswordsViewTest() {
  ON_CALL(*model_delegate_mock(), GetOrigin)
      .WillByDefault(Return(url::Origin::Create(CreateTestPasswordForm().url)));
  ON_CALL(*model_delegate_mock(), GetState)
      .WillByDefault(Return(password_manager::ui::MANAGE_STATE));
  ON_CALL(*model_delegate_mock(), GetCurrentForms)
      .WillByDefault(ReturnRef(current_forms_));
  ON_CALL(*model_delegate_mock(),
          GetManagePasswordsSingleCredentialDetailsModeCredential)
      .WillByDefault(ReturnRef(single_credential_details_mode_credential_));
}

void ManagePasswordsViewTest::CreateViewAndShow() {
  CreateAnchorViewAndShow();

  view_ = new ManagePasswordsView(web_contents(), anchor_view());
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
  view_widget_ = view_->GetWidget()->GetWeakPtr();
}

TEST_F(ManagePasswordsViewTest, HasTitle) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowWindowTitle());
}

TEST_F(ManagePasswordsViewTest,
       BubbleHasDetailsLayoutInSingleCredentialDetailsMode) {
  std::optional details_bubble_credentail{password_manager::PasswordForm()};
  ON_CALL(*model_delegate_mock(),
          GetManagePasswordsSingleCredentialDetailsModeCredential)
      .WillByDefault(ReturnRef(details_bubble_credentail));
  CreateViewAndShow();

  EXPECT_TRUE(view()->HasPasswordDetailsViewForTesting());
}

TEST_F(ManagePasswordsViewTest, DetailsBubbleIsClosedAfterAuthExpiration) {
  std::optional details_bubble_credentail{password_manager::PasswordForm()};
  ON_CALL(*model_delegate_mock(),
          GetManagePasswordsSingleCredentialDetailsModeCredential)
      .WillByDefault(ReturnRef(details_bubble_credentail));
  EXPECT_CALL(*model_delegate_mock(), OnBubbleHidden);
  CreateViewAndShow();

  // To avoid dangling pointer as the view can be destroyed on widget closing.
  ClearView();
  task_environment()->FastForwardBy(
      password_manager::constants::kPasswordManagerAuthValidity);
  Mock::VerifyAndClearExpectations(model_delegate_mock());
}

TEST_F(ManagePasswordsViewTest,
       DetailsBubbleHasBackButtonIfInCredentialListMode) {
  password_manager::PasswordForm details_bubble_credentail;
  CreateViewAndShow();
  view()->DisplayDetailsOfPasswordForTesting(password_manager::PasswordForm());

  // Imitate selection from the list.
  view()->DisplayDetailsOfPasswordForTesting(details_bubble_credentail);

  EXPECT_TRUE(PasswordDetailsHasBackButton());
}

TEST_F(ManagePasswordsViewTest,
       DetailsBubbleHasNoBackButtonIfInSingleCredentialMode) {
  std::optional details_bubble_credentail((password_manager::PasswordForm()));
  ON_CALL(*model_delegate_mock(),
          GetManagePasswordsSingleCredentialDetailsModeCredential)
      .WillByDefault(ReturnRef(details_bubble_credentail));
  CreateViewAndShow();

  EXPECT_FALSE(PasswordDetailsHasBackButton());
}

TEST_F(ManagePasswordsViewTest, DetailsBubbleTitleDependsOnFormDisplayName) {
  password_manager::PasswordForm form;
  form.signon_realm = "android://hash@com.netflix.mediaclient/";
  form.app_display_name = "Netflix";
  std::optional details_bubble_credentail(form);
  ON_CALL(*model_delegate_mock(),
          GetManagePasswordsSingleCredentialDetailsModeCredential)
      .WillByDefault(ReturnRef(details_bubble_credentail));
  CreateViewAndShow();

  EXPECT_EQ(view()->GetWindowTitle(), u"Netflix");
}

TEST_F(ManagePasswordsViewTest,
       PasswordDetailsFromListAllowsEmptyUsernameEdit) {
  password_manager::PasswordForm form;
  form.username_value = u"";
  form.password_value = u"pa$$w0rd";
  CreateViewAndShow();
  view()->DisplayDetailsOfPasswordForTesting(form);

  EXPECT_TRUE(PasswordDetailsHasEditUsernameButton());
}

TEST_F(ManagePasswordsViewTest, DetailsOnlyBubbleDoesntAllowEmptyUsernameEdit) {
  password_manager::PasswordForm form;
  form.username_value = u"";
  form.password_value = u"pa$$w0rd";
  std::optional details_bubble_credentail(form);
  ON_CALL(*model_delegate_mock(),
          GetManagePasswordsSingleCredentialDetailsModeCredential)
      .WillByDefault(ReturnRef(details_bubble_credentail));
  CreateViewAndShow();

  EXPECT_FALSE(PasswordDetailsHasEditUsernameButton());
}
