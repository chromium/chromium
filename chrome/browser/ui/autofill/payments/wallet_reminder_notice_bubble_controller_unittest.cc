// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace autofill {

namespace {

class MockAutofillBubbleHandler : public TestAutofillBubbleHandler {
 public:
  MOCK_METHOD(AutofillBubbleBase*,
              ShowWalletReminderNoticeBubble,
              (content::WebContents*,
               WalletReminderNoticeBubbleController*,
               bool),
              (override));
};

class WalletReminderNoticeBubbleControllerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  WalletReminderNoticeBubbleControllerTest() = default;
  ~WalletReminderNoticeBubbleControllerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("about:blank"));

    ON_CALL(mock_tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(tab_unowned_user_data_host_));
    ON_CALL(mock_tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(testing::Return(&mock_browser_window_interface_));
    ON_CALL(mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(browser_unowned_user_data_host_));

    test_autofill_bubble_handler_registration_ =
        std::make_unique<ui::ScopedUnownedUserData<AutofillBubbleHandler>>(
            mock_browser_window_interface_.GetUnownedUserDataHost(),
            mock_autofill_bubble_handler_);

    controller_ = std::make_unique<WalletReminderNoticeBubbleController>(
        mock_tab_interface_, web_contents());
  }

  void TearDown() override {
    test_autofill_bubble_handler_registration_.reset();
    controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  tabs::MockTabInterface mock_tab_interface_;
  MockBrowserWindowInterface mock_browser_window_interface_;
  ui::UnownedUserDataHost browser_unowned_user_data_host_;
  MockAutofillBubbleHandler mock_autofill_bubble_handler_;
  std::unique_ptr<ui::ScopedUnownedUserData<AutofillBubbleHandler>>
      test_autofill_bubble_handler_registration_;
  TestAutofillBubble test_bubble_;
  ui::UnownedUserDataHost tab_unowned_user_data_host_;
  std::unique_ptr<WalletReminderNoticeBubbleController> controller_;
};

TEST_F(WalletReminderNoticeBubbleControllerTest, From) {
  EXPECT_EQ(WalletReminderNoticeBubbleController::From(mock_tab_interface_),
            controller_.get());
}

TEST_F(WalletReminderNoticeBubbleControllerTest, Show_ShowsBubble) {
  EXPECT_CALL(mock_autofill_bubble_handler_,
              ShowWalletReminderNoticeBubble(web_contents(), controller_.get(),
                                             /*is_user_gesture=*/false))
      .WillOnce(testing::Return(&test_bubble_));

  EXPECT_EQ(controller_->GetBubbleView(), nullptr);
  controller_->Show({TestLegalMessageLine("Line 1")});
  EXPECT_EQ(controller_->GetBubbleView(), &test_bubble_);
}

TEST_F(WalletReminderNoticeBubbleControllerTest, ReshowBubble_ShowsBubble) {
  EXPECT_CALL(mock_autofill_bubble_handler_,
              ShowWalletReminderNoticeBubble(web_contents(), controller_.get(),
                                             /*is_user_gesture=*/true))
      .WillOnce(testing::Return(&test_bubble_));

  EXPECT_EQ(controller_->GetBubbleView(), nullptr);
  controller_->ReshowBubble();
  EXPECT_EQ(controller_->GetBubbleView(), &test_bubble_);
}

TEST_F(WalletReminderNoticeBubbleControllerTest, GetWindowTitle) {
  EXPECT_EQ(
      controller_->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_WALLET_REMINDER_NOTICE_TITLE));
}

TEST_F(WalletReminderNoticeBubbleControllerTest, GetLegalMessageLines) {
  LegalMessageLines legal_message_lines = {TestLegalMessageLine("Line 1."),
                                           TestLegalMessageLine("Line 2.")};

  EXPECT_TRUE(controller_->GetLegalMessageLines().empty());
  controller_->Show(legal_message_lines);
  EXPECT_EQ(controller_->GetLegalMessageLines(), legal_message_lines);
}

TEST_F(WalletReminderNoticeBubbleControllerTest, GetBubbleType) {
  EXPECT_EQ(controller_->GetBubbleType(), BubbleType::kWalletReminderNotice);
}

TEST_F(WalletReminderNoticeBubbleControllerTest,
       GetBubbleControllerBaseWeakPtr) {
  base::WeakPtr<BubbleControllerBase> weak_ptr =
      controller_->GetBubbleControllerBaseWeakPtr();
  EXPECT_EQ(weak_ptr.get(), controller_.get());

  controller_.reset();
  EXPECT_EQ(weak_ptr.get(), nullptr);
}

}  // namespace

}  // namespace autofill
