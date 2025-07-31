// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/save_to_drive/account_chooser_view.h"

#include "chrome/browser/ui/save_to_drive/mock_account_chooser_controller_delegate.h"
#include "chrome/browser/ui/save_to_drive/mock_account_chooser_view_delegate.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_radio_group_view.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_test_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/views_test_base.h"

namespace save_to_drive {
namespace {

const char kTestDomain[] = "test.com";

using ::save_to_drive::testing::GetTestAccounts;
using ::save_to_drive::testing::VerifyAccountChooserRow;

void VerifyAccountChooserViewHeader(views::View* header_view,
                                    std::u16string expected_title) {
  views::Label* title_label = static_cast<views::Label*>(
      header_view->children().front()->children().front());
  ASSERT_TRUE(title_label);
  EXPECT_EQ(title_label->GetText(), expected_title);
  views::StyledLabel* subtitle_label =
      static_cast<views::StyledLabel*>(header_view->children().back());
  // Verify the subtitle label.
  ASSERT_TRUE(subtitle_label);
}

void VerifyAccountChooserViewFooter(
    views::View* footer_view,
    std::u16string expected_use_other_account_button_text,
    std::u16string expected_cancel_button_text,
    std::u16string expected_save_button_text) {
  std::vector<raw_ptr<views::View, VectorExperimental>> footer_view_children =
      footer_view->children();
  ASSERT_EQ(footer_view_children.size(), 3u);
  // Use other account button.
  views::MdTextButton* use_other_account_button =
      static_cast<views::MdTextButton*>(
          footer_view_children.front()->children().front());
  ASSERT_TRUE(use_other_account_button);
  EXPECT_EQ(use_other_account_button->GetText(),
            expected_use_other_account_button_text);
  // Cancel button.
  views::MdTextButton* cancel_button =
      static_cast<views::MdTextButton*>(footer_view_children.at(1));
  ASSERT_TRUE(cancel_button);
  EXPECT_EQ(cancel_button->GetText(), expected_cancel_button_text);
  // Save button.
  views::MdTextButton* save_button =
      static_cast<views::MdTextButton*>(footer_view_children.at(2));
  ASSERT_TRUE(save_button);
  EXPECT_EQ(save_button->GetText(), expected_save_button_text);
}

void TestSingleAccount(AccountChooserView* account_chooser_view,
                       const AccountInfo& account) {
  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      account_chooser_view->children();
  ASSERT_EQ(children.size(), 3u);  // header + body + footer

  // check header contents
  VerifyAccountChooserViewHeader(
      children.at(0),
      l10n_util::GetStringUTF16(IDS_ACCOUNT_CHOOSER_SINGLE_ACCOUNT_TITLE));

  // check body contents
  views::View* body_view = children.at(1);
  std::vector<raw_ptr<views::View, VectorExperimental>> body_view_children =
      body_view->children();
  ASSERT_EQ(body_view_children.size(), 3u);  // 2 separators + account
  EXPECT_TRUE(VerifyAccountChooserRow(
      // Extra flex layout view around the account row for correct spacing.
      body_view_children.at(1)->children().front(), account));

  // check footer contents
  VerifyAccountChooserViewFooter(
      children.at(2),
      l10n_util::GetStringUTF16(IDS_ACCOUNT_CHOOSER_ADD_ACCOUNT),
      l10n_util::GetStringUTF16(IDS_CANCEL),
      l10n_util::GetStringUTF16(IDS_SAVE));
}

void TestMultiAccount(AccountChooserView* account_chooser_view,
                      const std::vector<AccountInfo>& accounts) {
  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      account_chooser_view->children();
  ASSERT_EQ(children.size(), 3u);  // header + body + footer

  // check header contents
  VerifyAccountChooserViewHeader(
      children.at(0),
      l10n_util::GetStringUTF16(IDS_ACCOUNT_CHOOSER_MULTI_ACCOUNT_TITLE));

  // check body contents
  AccountChooserRadioGroupView* body_view =
      static_cast<AccountChooserRadioGroupView*>(
          // body view has one level of abstraction for easy updating and is
          // wrapped by a scroll view.
          children.at(1)->children().front());
  ASSERT_TRUE(body_view);

  // check footer contents
  VerifyAccountChooserViewFooter(
      children.at(2),
      l10n_util::GetStringUTF16(IDS_ACCOUNT_CHOOSER_ADD_ACCOUNT),
      l10n_util::GetStringUTF16(IDS_CANCEL),
      l10n_util::GetStringUTF16(IDS_SAVE));
}

class AccountChooserViewTest : public views::ViewsTestBase {
 public:
  AccountChooserViewTest() = default;

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
  MockAccountChooserControllerDelegate
      mock_account_chooser_controller_delegate_;
  MockAccountChooserViewDelegate mock_account_chooser_view_delegate_;
};

TEST_F(AccountChooserViewTest, SingleAccount) {
  std::vector<AccountInfo> accounts = GetTestAccounts({"pothos"}, kTestDomain);
  AccountChooserView* account_chooser_view =
      anchor_view_->AddChildView(std::make_unique<AccountChooserView>(
          &mock_account_chooser_controller_delegate_,
          &mock_account_chooser_view_delegate_, accounts, std::nullopt));
  TestSingleAccount(account_chooser_view, accounts.front());
}

// TODO(crbug.com/435260088): Re-enable this test
#if BUILDFLAG(IS_LINUX)
#define MAYBE_MultiAccount DISABLED_MultiAccount
#else
#define MAYBE_MultiAccount MultiAccount
#endif
TEST_F(AccountChooserViewTest, MAYBE_MultiAccount) {
  std::vector<AccountInfo> accounts =
      GetTestAccounts({"pothos", "fern"}, kTestDomain);
  AccountChooserView* account_chooser_view =
      anchor_view_->AddChildView(std::make_unique<AccountChooserView>(
          &mock_account_chooser_controller_delegate_,
          &mock_account_chooser_view_delegate_, accounts, std::nullopt));
  TestMultiAccount(account_chooser_view, accounts);
}

// TODO(crbug.com/435260088): Re-enable this test
#if BUILDFLAG(IS_LINUX)
#define MAYBE_SingleToMultiAccountViewUpdate \
  DISABLED_SingleToMultiAccountViewUpdate
#else
#define MAYBE_SingleToMultiAccountViewUpdate SingleToMultiAccountViewUpdate
#endif
TEST_F(AccountChooserViewTest, MAYBE_SingleToMultiAccountViewUpdate) {
  std::vector<AccountInfo> accounts = GetTestAccounts({"pothos"}, kTestDomain);
  AccountChooserView* account_chooser_view =
      anchor_view_->AddChildView(std::make_unique<AccountChooserView>(
          &mock_account_chooser_controller_delegate_,
          &mock_account_chooser_view_delegate_, accounts, std::nullopt));
  TestSingleAccount(account_chooser_view, accounts.front());
  std::vector<AccountInfo> new_accounts =
      GetTestAccounts({"pothos", "fern"}, kTestDomain);
  account_chooser_view->UpdateView(new_accounts, std::nullopt);
  TestMultiAccount(account_chooser_view, new_accounts);
}

// TODO(crbug.com/435260088): Re-enable this test
#if BUILDFLAG(IS_LINUX)
#define MAYBE_MultiToSingleAccountViewUpdate \
  DISABLED_MultiToSingleAccountViewUpdate
#else
#define MAYBE_MultiToSingleAccountViewUpdate MultiToSingleAccountViewUpdate
#endif
TEST_F(AccountChooserViewTest, MAYBE_MultiToSingleAccountViewUpdate) {
  std::vector<AccountInfo> accounts =
      GetTestAccounts({"pothos", "fern"}, kTestDomain);
  AccountChooserView* account_chooser_view =
      anchor_view_->AddChildView(std::make_unique<AccountChooserView>(
          &mock_account_chooser_controller_delegate_,
          &mock_account_chooser_view_delegate_, accounts, std::nullopt));
  TestMultiAccount(account_chooser_view, accounts);
  std::vector<AccountInfo> new_accounts =
      GetTestAccounts({"pothos"}, kTestDomain);
  account_chooser_view->UpdateView(new_accounts, std::nullopt);
  TestSingleAccount(account_chooser_view, new_accounts.front());
}

}  // namespace
}  // namespace save_to_drive
