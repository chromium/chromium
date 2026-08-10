// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"

#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace autofill {

namespace {

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

    controller_ = std::make_unique<WalletReminderNoticeBubbleController>(
        mock_tab_interface_, web_contents());
  }

  void TearDown() override {
    controller_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  tabs::MockTabInterface mock_tab_interface_;
  ui::UnownedUserDataHost tab_unowned_user_data_host_;
  std::unique_ptr<WalletReminderNoticeBubbleController> controller_;
};

TEST_F(WalletReminderNoticeBubbleControllerTest, From) {
  EXPECT_EQ(WalletReminderNoticeBubbleController::From(mock_tab_interface_),
            controller_.get());
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
