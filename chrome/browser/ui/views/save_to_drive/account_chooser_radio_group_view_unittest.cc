// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/save_to_drive/account_chooser_radio_group_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_test_util.h"
#include "chrome/browser/ui/views/save_to_drive/mock_account_chooser_radio_button_delegate.h"
#include "chrome/browser/ui/views/save_to_drive/mock_account_chooser_view_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"

namespace save_to_drive {
namespace {

const char kTestDomain[] = "test.com";

using ::save_to_drive::testing::GetTestAccount;
using ::save_to_drive::testing::VerifyAccountChooserRow;
using ::testing::Field;

void VerifyAccountChooserRadioButtonRow(views::View* row_view,
                                        const AccountInfo& account,
                                        bool is_selected) {
  EXPECT_TRUE(VerifyAccountChooserRow(
      row_view->children().front()->children().front(), account));
  views::RadioButton* radio_button =
      static_cast<views::RadioButton*>(row_view->children().back());
  ASSERT_TRUE(radio_button);
  EXPECT_EQ(radio_button->GetChecked(), is_selected);
}

class AccountChooserRadioGroupViewTest : public views::ViewsTestBase {
 public:
  AccountChooserRadioGroupViewTest() = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    // Makes ChromeLayoutProvider available through the static
    // ChromeLayoutProvider::Get() accessor.
    test_views_delegate()->set_layout_provider(
        ChromeLayoutProvider::CreateLayoutProvider());

    // Create the anchor Widget.
    anchor_view_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    anchor_view_widget_->Show();
    anchor_view_ =
        anchor_view_widget_->SetContentsView(std::make_unique<views::View>());
  }

  void TearDown() override {
    anchor_view_ = nullptr;
    anchor_view_widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> anchor_view_widget_;
  raw_ptr<views::View> anchor_view_;
  MockAccountChooserViewDelegate mock_account_chooser_view_delegate_;
};

TEST_F(AccountChooserRadioGroupViewTest, MultiAccountWithoutPrimary) {
  AccountInfo account_pothos =
      GetTestAccount("pothos", kTestDomain, /*gaia_id=*/1);
  AccountInfo account_fern = GetTestAccount("fern", kTestDomain, /*gaia_id=*/2);
  std::vector<AccountInfo> accounts = {account_pothos, account_fern};

  // We expect the lexicographically first account to be selected.
  EXPECT_CALL(mock_account_chooser_view_delegate_,
              OnAccountSelected(
                  Field(&AccountInfo::account_id, account_fern.account_id)));
  AccountChooserRadioGroupView* account_chooser_view =
      anchor_view_->AddChildView(std::make_unique<AccountChooserRadioGroupView>(
          mock_account_chooser_view_delegate_, accounts, std::nullopt));
  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      account_chooser_view->children();
  ASSERT_EQ(children.size(), 5u);  // 3 separators + 2 accounts
  // Accounts are lexicographically ordered by full name.
  ASSERT_EQ(children[1]->GetClassName(), "AccountChooserRadioButtonRow");
  // In the absence of a primary account, the first account is selected by
  // default.
  VerifyAccountChooserRadioButtonRow(children[1], account_fern,
                                     /*is_selected=*/true);
  EXPECT_EQ(children[3]->GetClassName(), "AccountChooserRadioButtonRow");
  VerifyAccountChooserRadioButtonRow(children[3], account_pothos,
                                     /*is_selected=*/false);
}

TEST_F(AccountChooserRadioGroupViewTest, MultiAccountWithPrimary) {
  AccountInfo account_pothos =
      GetTestAccount("pothos", kTestDomain, /*gaia_id=*/1);
  AccountInfo account_fern = GetTestAccount("fern", kTestDomain, /*gaia_id=*/2);
  AccountInfo account_alder =
      GetTestAccount("alder", kTestDomain, /*gaia_id=*/3);
  std::vector<AccountInfo> accounts = {account_fern, account_pothos,
                                       account_alder};
  // We expect the primary account to be selected.
  EXPECT_CALL(mock_account_chooser_view_delegate_,
              OnAccountSelected(
                  Field(&AccountInfo::account_id, account_pothos.account_id)));
  AccountChooserRadioGroupView* account_chooser_view =
      anchor_view_->AddChildView(std::make_unique<AccountChooserRadioGroupView>(
          mock_account_chooser_view_delegate_, accounts,
          account_pothos.account_id));
  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      account_chooser_view->children();
  ASSERT_EQ(children.size(), 7u);  // 4 separators + 3 accounts
  // Primary account is first.
  ASSERT_EQ(children[1]->GetClassName(), "AccountChooserRadioButtonRow");
  VerifyAccountChooserRadioButtonRow(children[1], account_pothos,
                                     /*is_selected=*/true);
  // After the primary account, the accounts are ordered lexicographically by
  // full name.
  EXPECT_EQ(children[3]->GetClassName(), "AccountChooserRadioButtonRow");
  VerifyAccountChooserRadioButtonRow(children[3], account_alder,
                                     /*is_selected=*/false);
  EXPECT_EQ(children[5]->GetClassName(), "AccountChooserRadioButtonRow");
  VerifyAccountChooserRadioButtonRow(children[5], account_fern,
                                     /*is_selected=*/false);
}

TEST_F(AccountChooserRadioGroupViewTest,
       AccountChooserRadioButtonRowClickInvokesDelegate) {
  MockAccountChooserRadioButtonDelegate delegate;
  AccountInfo account = GetTestAccount("account", kTestDomain, /*gaia_id=*/1);
  AccountChooserRadioButtonRow* row_view = anchor_view_->AddChildView(
      std::make_unique<AccountChooserRadioButtonRow>(&delegate, account));

  EXPECT_CALL(delegate, SelectAccount(Field(&AccountInfo::account_id,
                                            account.account_id)));

  ui::MouseEvent event(ui::EventType::kMousePressed, /*location=*/gfx::PointF{},
                       /*root_location=*/gfx::PointF{},
                       /*time_stamp=*/{}, ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
  row_view->OnMousePressed(event);
}

TEST_F(AccountChooserRadioGroupViewTest, MultiAccountWithSameName) {
  AccountInfo account_pothos =
      GetTestAccount("pothos", kTestDomain, /*gaia_id=*/1);
  AccountInfo account_pothos2 =
      GetTestAccount("pothos", /*domain=*/"test2.com", /*gaia_id=*/2);
  std::vector<AccountInfo> accounts = {account_pothos2, account_pothos};

  // We expect the lexicographically first account to be selected.
  EXPECT_CALL(mock_account_chooser_view_delegate_,
              OnAccountSelected(
                  Field(&AccountInfo::account_id, account_pothos.account_id)));
  AccountChooserRadioGroupView* account_chooser_view =
      anchor_view_->AddChildView(std::make_unique<AccountChooserRadioGroupView>(
          mock_account_chooser_view_delegate_, accounts, std::nullopt));
  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      account_chooser_view->children();
  ASSERT_EQ(children.size(), 5u);  // 3 separators + 2 accounts
  // Accounts are lexicographically ordered by full name, then email.
  ASSERT_EQ(children[1]->GetClassName(), "AccountChooserRadioButtonRow");
  // In the absence of a primary account, the first account is selected by
  // default.
  VerifyAccountChooserRadioButtonRow(children[1], account_pothos,
                                     /*is_selected=*/true);
  EXPECT_EQ(children[3]->GetClassName(), "AccountChooserRadioButtonRow");
  VerifyAccountChooserRadioButtonRow(children[3], account_pothos2,
                                     /*is_selected=*/false);
}

TEST_F(AccountChooserRadioGroupViewTest,
       AccountChooserRadioButtonClickInvokesDelegate) {
  MockAccountChooserRadioButtonDelegate delegate;
  AccountInfo account = GetTestAccount("account", kTestDomain, /*gaia_id=*/1);
  AccountChooserRadioButtonRow* row_view = anchor_view_->AddChildView(
      std::make_unique<AccountChooserRadioButtonRow>(&delegate, account));

  EXPECT_CALL(delegate, SelectAccount(Field(&AccountInfo::account_id,
                                            account.account_id)));

  views::RadioButton* radio_button =
      static_cast<views::RadioButton*>(row_view->children().back());
  ASSERT_TRUE(radio_button);
  radio_button->SetChecked(true);
}
}  // namespace
}  // namespace save_to_drive
