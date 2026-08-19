// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_ui_delegate_desktop.h"

#include <memory>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_page_action_controller.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace autofill::payments {

class WalletReminderNoticeUiDelegateDesktopTest
    : public ChromeRenderViewHostTestHarness {
 public:
  WalletReminderNoticeUiDelegateDesktopTest() = default;
  ~WalletReminderNoticeUiDelegateDesktopTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("about:blank"));

    // Set up the mock tab interface so MaybeGetFromContents can find it.
    tabs::TabLookupFromWebContents::CreateForWebContents(web_contents(),
                                                         &mock_tab_interface_);

    // The controllers attach themselves to the TabInterface via
    // UnownedUserData.
    ON_CALL(mock_tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(tab_unowned_user_data_host_));

    // Instantiate controllers. In production these are owned by TabFeatures.
    bubble_controller_ = std::make_unique<WalletReminderNoticeBubbleController>(
        mock_tab_interface_, web_contents());
    mock_page_action_controller_ =
        std::make_unique<page_actions::MockPageActionController>();
    page_action_controller_ =
        std::make_unique<WalletReminderNoticePageActionController>(
            mock_tab_interface_, *mock_page_action_controller_);

    // Inject the Autofill client.
    client_ = std::make_unique<TestContentAutofillClient>(web_contents());
    delegate_ =
        std::make_unique<WalletReminderNoticeUiDelegateDesktop>(client_.get());
  }

  void TearDown() override {
    delegate_.reset();
    client_.reset();
    page_action_controller_.reset();
    mock_page_action_controller_.reset();
    bubble_controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  tabs::MockTabInterface& tab_interface() { return mock_tab_interface_; }

 protected:
  ui::UnownedUserDataHost tab_unowned_user_data_host_;
  tabs::MockTabInterface mock_tab_interface_;

  std::unique_ptr<WalletReminderNoticeBubbleController> bubble_controller_;
  std::unique_ptr<page_actions::MockPageActionController>
      mock_page_action_controller_;
  std::unique_ptr<WalletReminderNoticePageActionController>
      page_action_controller_;

  std::unique_ptr<TestContentAutofillClient> client_;
  std::unique_ptr<WalletReminderNoticeUiDelegateDesktop> delegate_;
};

TEST_F(WalletReminderNoticeUiDelegateDesktopTest, ShowWalletReminderNotice) {
  LegalMessageLines legal_message_lines = {TestLegalMessageLine("Line 1.")};

  auto* bubble_controller =
      WalletReminderNoticeBubbleController::From(tab_interface());
  ASSERT_TRUE(bubble_controller);
  EXPECT_TRUE(bubble_controller->GetLegalMessageLines().empty());

  EXPECT_CALL(*mock_page_action_controller_, Show(kActionWalletReminderNotice));

  delegate_->ShowWalletReminderNotice(legal_message_lines);

  // Verify that the legal message lines are forwarded to the bubble controller.
  EXPECT_EQ(bubble_controller->GetLegalMessageLines(), legal_message_lines);
}

}  // namespace autofill::payments
