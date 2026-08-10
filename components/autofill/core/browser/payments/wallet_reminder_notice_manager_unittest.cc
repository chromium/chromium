// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/wallet_reminder_notice_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/ui/payments/wallet_reminder_notice_ui_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

using ::testing::_;

class MockWalletReminderNoticeUiDelegate
    : public WalletReminderNoticeUiDelegate {
 public:
  MOCK_METHOD(void,
              ShowWalletReminderNotice,
              (LegalMessageLines legal_message_lines),
              (override));
};

class WalletReminderNoticeManagerTest : public testing::Test {
 public:
  WalletReminderNoticeManagerTest() : manager_(&autofill_client_) {
    auto ui_delegate = std::make_unique<MockWalletReminderNoticeUiDelegate>();
    ui_delegate_ = ui_delegate.get();
    autofill_client_.GetPaymentsAutofillClient()
        ->set_wallet_reminder_notice_ui_delegate(std::move(ui_delegate));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  TestAutofillClient autofill_client_;
  raw_ptr<MockWalletReminderNoticeUiDelegate> ui_delegate_;
  WalletReminderNoticeManager manager_;
};

TEST_F(WalletReminderNoticeManagerTest, ShowWalletReminderNotice) {
  EXPECT_CALL(*ui_delegate_, ShowWalletReminderNotice(_)).Times(0);

  manager_.ShowWalletReminderNotice();
}

TEST_F(WalletReminderNoticeManagerTest,
       OnGetWalletReminderNoticeResponse_UserAlreadyAcknowledgedOnServer) {
  EXPECT_CALL(*ui_delegate_, ShowWalletReminderNotice(_)).Times(0);

  LegalMessageLines legal_message_lines;
  legal_message_lines.push_back(TestLegalMessageLine("Test Legal Notice"));
  manager_.OnGetWalletReminderNoticeResponse(legal_message_lines, "token",
                                             /*has_user_acknowledged=*/true);
}

TEST_F(WalletReminderNoticeManagerTest,
       OnGetWalletReminderNoticeResponse_EmptyLegalMessage) {
  EXPECT_CALL(*ui_delegate_, ShowWalletReminderNotice(_)).Times(0);

  manager_.OnGetWalletReminderNoticeResponse(LegalMessageLines(), "token",
                                             /*has_user_acknowledged=*/false);
}

TEST_F(WalletReminderNoticeManagerTest,
       OnGetWalletReminderNoticeResponse_EmptyToken) {
  EXPECT_CALL(*ui_delegate_, ShowWalletReminderNotice(_)).Times(0);

  LegalMessageLines legal_message_lines;
  legal_message_lines.push_back(TestLegalMessageLine("Test Legal Notice"));
  manager_.OnGetWalletReminderNoticeResponse(legal_message_lines, "",
                                             /*has_user_acknowledged=*/false);
}

TEST_F(WalletReminderNoticeManagerTest,
       OnGetWalletReminderNoticeResponse_SuccessShowsNotice) {
  EXPECT_CALL(*ui_delegate_, ShowWalletReminderNotice(_)).Times(1);

  LegalMessageLines legal_message_lines;
  legal_message_lines.push_back(TestLegalMessageLine("Test Legal Notice"));
  manager_.OnGetWalletReminderNoticeResponse(legal_message_lines, "token",
                                             /*has_user_acknowledged=*/false);
}

}  // namespace
}  // namespace autofill::payments
