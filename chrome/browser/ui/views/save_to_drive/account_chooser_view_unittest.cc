// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/save_to_drive/account_chooser_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_radio_group_view.h"
#include "chrome/browser/ui/views/save_to_drive/account_chooser_test_util.h"
#include "chrome/browser/ui/views/save_to_drive/mock_account_chooser_view_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view_utils.h"

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

void VerifyAccountChooserViewFooter(views::View* footer_view) {
  ui::ElementContext context =
      views::ElementTrackerViews::GetContextForView(footer_view);
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetUniqueElement(
      AccountChooserView::kAddAccountButtonId, context));
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetUniqueElement(
      AccountChooserView::kCancelButtonId, context));
  EXPECT_TRUE(ui::ElementTracker::GetElementTracker()->GetUniqueElement(
      AccountChooserView::kSaveButtonId, context));
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
  VerifyAccountChooserViewFooter(children.at(2));
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

  // Check body contents.  AccountChooserRadioGroupView is contained within the
  // scroll view.  While it is possible to obtain using
  // children.at(1)->children().front()->children().front(), this approach is
  // brittle because it depends on the implementation of ScrollView.
  EXPECT_TRUE(views::IsViewClass<views::ScrollView>(children.at(1)));

  // check footer contents
  VerifyAccountChooserViewFooter(children.at(2));
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
  MockAccountChooserViewDelegate mock_account_chooser_view_delegate_;
};

TEST_F(AccountChooserViewTest, SingleAccount) {
  std::vector<AccountInfo> accounts = GetTestAccounts({"pothos"}, kTestDomain);
  AccountChooserView* account_chooser_view =
      anchor_view_->AddChildView(std::make_unique<AccountChooserView>(
          &mock_account_chooser_view_delegate_, accounts, std::nullopt));
  TestSingleAccount(account_chooser_view, accounts.front());
}

TEST_F(AccountChooserViewTest, MultiAccount) {
  std::vector<AccountInfo> accounts =
      GetTestAccounts({"pothos", "fern"}, kTestDomain);
  AccountChooserView* account_chooser_view =
      anchor_view_->AddChildView(std::make_unique<AccountChooserView>(
          &mock_account_chooser_view_delegate_, accounts, std::nullopt));
  TestMultiAccount(account_chooser_view, accounts);
}

TEST_F(AccountChooserViewTest, SingleToMultiAccountViewUpdate) {
  std::vector<AccountInfo> accounts = GetTestAccounts({"pothos"}, kTestDomain);
  AccountChooserView* account_chooser_view =
      anchor_view_->AddChildView(std::make_unique<AccountChooserView>(
          &mock_account_chooser_view_delegate_, accounts, std::nullopt));
  TestSingleAccount(account_chooser_view, accounts.front());
  std::vector<AccountInfo> new_accounts =
      GetTestAccounts({"pothos", "fern"}, kTestDomain);
  account_chooser_view->UpdateView(new_accounts, std::nullopt);
  TestMultiAccount(account_chooser_view, new_accounts);
}

TEST_F(AccountChooserViewTest, MultiToSingleAccountViewUpdate) {
  std::vector<AccountInfo> accounts =
      GetTestAccounts({"pothos", "fern"}, kTestDomain);
  AccountChooserView* account_chooser_view =
      anchor_view_->AddChildView(std::make_unique<AccountChooserView>(
          &mock_account_chooser_view_delegate_, accounts, std::nullopt));
  TestMultiAccount(account_chooser_view, accounts);
  std::vector<AccountInfo> new_accounts =
      GetTestAccounts({"pothos"}, kTestDomain);
  account_chooser_view->UpdateView(new_accounts, std::nullopt);
  TestSingleAccount(account_chooser_view, new_accounts.front());
}

}  // namespace
}  // namespace save_to_drive
