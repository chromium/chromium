// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client_android.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/android/autofill/autofill_wallet_reminder_notice_bottom_sheet_bridge.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client_android_test_api.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {

class MockAutofillWalletReminderNoticeBottomSheetBridge
    : public AutofillWalletReminderNoticeBottomSheetBridge {
 public:
  MockAutofillWalletReminderNoticeBottomSheetBridge()
      : AutofillWalletReminderNoticeBottomSheetBridge(
            /*window_android=*/nullptr) {}
  ~MockAutofillWalletReminderNoticeBottomSheetBridge() override = default;

  MOCK_METHOD(void, RequestShowContent, (LegalMessageLines), (override));
};

class ChromePaymentsAutofillClientAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromePaymentsAutofillClientAndroidTest() = default;
  ~ChromePaymentsAutofillClientAndroidTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ChromeAutofillClient::CreateForWebContents(web_contents());
  }

  ChromeAutofillClient* client() {
    return ChromeAutofillClient::FromWebContentsForTesting(web_contents());
  }

  ChromePaymentsAutofillClientAndroid* chrome_payments_client_android() {
    return static_cast<ChromePaymentsAutofillClientAndroid*>(
        client()->GetPaymentsAutofillClient());
  }
};

TEST_F(ChromePaymentsAutofillClientAndroidTest, ShowWalletReminderNotice) {
  auto mock_bridge =
      std::make_unique<MockAutofillWalletReminderNoticeBottomSheetBridge>();
  EXPECT_CALL(
      *mock_bridge,
      RequestShowContent(testing::ElementsAre(testing::Property(
          &LegalMessageLine::text, testing::Eq(u"Test Legal Message")))))
      .Times(1);

  test_api(*chrome_payments_client_android())
      .SetAutofillWalletReminderNoticeBottomSheetBridge(std::move(mock_bridge));

  LegalMessageLines legal_message_lines;
  legal_message_lines.push_back(TestLegalMessageLine("Test Legal Message"));
  chrome_payments_client_android()->ShowWalletReminderNotice(
      std::move(legal_message_lines));
}

}  // namespace autofill::payments
