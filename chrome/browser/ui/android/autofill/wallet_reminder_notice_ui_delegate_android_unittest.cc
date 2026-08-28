// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/wallet_reminder_notice_ui_delegate_android.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/android/autofill/autofill_wallet_reminder_notice_bottom_sheet_bridge.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

class MockAutofillWalletReminderNoticeBottomSheetBridge
    : public AutofillWalletReminderNoticeBottomSheetBridge {
 public:
  MockAutofillWalletReminderNoticeBottomSheetBridge()
      : AutofillWalletReminderNoticeBottomSheetBridge(
            /*window_android=*/nullptr) {}
  ~MockAutofillWalletReminderNoticeBottomSheetBridge() override = default;

  MOCK_METHOD(void, RequestShowContent, (LegalMessageLines), (override));
};

class WalletReminderNoticeUiDelegateAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  WalletReminderNoticeUiDelegateAndroidTest() = default;
  ~WalletReminderNoticeUiDelegateAndroidTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ChromeAutofillClient::CreateForWebContents(web_contents());
    delegate_ = std::make_unique<WalletReminderNoticeUiDelegateAndroid>(
        autofill_client());
  }

  ChromeAutofillClient* autofill_client() {
    return ChromeAutofillClient::FromWebContentsForTesting(web_contents());
  }

  WalletReminderNoticeUiDelegateAndroid* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<WalletReminderNoticeUiDelegateAndroid> delegate_;
};

TEST_F(WalletReminderNoticeUiDelegateAndroidTest, ShowWalletReminderNotice) {
  auto mock_bridge =
      std::make_unique<MockAutofillWalletReminderNoticeBottomSheetBridge>();
  EXPECT_CALL(
      *mock_bridge,
      RequestShowContent(testing::ElementsAre(testing::Property(
          &LegalMessageLine::text, testing::Eq(u"Test Legal Message")))))
      .Times(1);

  delegate()->SetAutofillWalletReminderNoticeBottomSheetBridgeForTesting(
      std::move(mock_bridge));

  LegalMessageLines legal_message_lines;
  legal_message_lines.push_back(TestLegalMessageLine("Test Legal Message"));
  delegate()->ShowWalletReminderNotice(std::move(legal_message_lines));
}

}  // namespace
}  // namespace autofill::payments
