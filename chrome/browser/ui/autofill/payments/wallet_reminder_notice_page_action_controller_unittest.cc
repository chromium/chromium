// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_page_action_controller.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::ReturnRef;

namespace autofill {

class WalletReminderNoticePageActionControllerTest : public testing::Test {
 public:
  WalletReminderNoticePageActionControllerTest() {
    ON_CALL(tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(Return(&mock_browser_window_interface_));
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    wallet_reminder_notice_page_action_controller_ =
        std::make_unique<WalletReminderNoticePageActionController>(
            tab_interface_, page_action_controller_);
  }

  WalletReminderNoticePageActionControllerTest(
      const WalletReminderNoticePageActionControllerTest&) = delete;
  WalletReminderNoticePageActionControllerTest& operator=(
      const WalletReminderNoticePageActionControllerTest&) = delete;

  ~WalletReminderNoticePageActionControllerTest() override = default;

  tabs::MockTabInterface& tab() { return tab_interface_; }

  page_actions::MockPageActionController& page_action_controller() {
    return page_action_controller_;
  }

  WalletReminderNoticePageActionController&
  wallet_reminder_notice_page_action_controller() {
    return *wallet_reminder_notice_page_action_controller_;
  }

 private:
  MockBrowserWindowInterface mock_browser_window_interface_;
  tabs::MockTabInterface tab_interface_;
  ui::UnownedUserDataHost user_data_host_;
  page_actions::MockPageActionController page_action_controller_;
  std::unique_ptr<WalletReminderNoticePageActionController>
      wallet_reminder_notice_page_action_controller_;
};

TEST_F(WalletReminderNoticePageActionControllerTest, FromReturnsController) {
  EXPECT_EQ(&wallet_reminder_notice_page_action_controller(),
            WalletReminderNoticePageActionController::From(tab()));
}

TEST_F(WalletReminderNoticePageActionControllerTest,
       ShowCallsPageActionController) {
  EXPECT_CALL(page_action_controller(), Show(kActionWalletReminderNotice))
      .Times(1);

  wallet_reminder_notice_page_action_controller().Show();
}

TEST_F(WalletReminderNoticePageActionControllerTest,
       HideCallsPageActionController) {
  EXPECT_CALL(page_action_controller(), Hide(kActionWalletReminderNotice))
      .Times(1);

  wallet_reminder_notice_page_action_controller().Hide();
}

}  // namespace autofill
