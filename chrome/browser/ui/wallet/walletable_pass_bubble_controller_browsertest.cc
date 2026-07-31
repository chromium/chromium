// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/wallet/walletable_pass_consent_bubble_controller.h"
#include "chrome/browser/ui/wallet/walletable_pass_save_bubble_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace wallet {
namespace {

class WalletablePassBubbleControllerBrowserTest : public InProcessBrowserTest {
 public:
  WalletablePassBubbleControllerBrowserTest() = default;
  ~WalletablePassBubbleControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }
};

// Tests that the save bubble is reshown when the tab is reactivated after
// clicking "Go to wallet" link.
IN_PROC_BROWSER_TEST_F(WalletablePassBubbleControllerBrowserTest,
                       ReshowSaveBubbleOnTabActivationAfterGoToWallet) {
  auto controller = std::make_unique<WalletablePassSaveBubbleController>(
      browser()->tab_strip_model()->GetTabAtIndex(0));
  wallet::WalletPass pass;
  wallet::LoyaltyCard loyalty_card;
  loyalty_card.plan_name = "Test Plan";
  loyalty_card.issuer_name = "Test Issuer";
  pass.pass_data = std::move(loyalty_card);

  controller->SetUpAndShowSaveBubble(pass, base::DoNothing());

  EXPECT_TRUE(controller->IsShowingBubble());

  // Simulate "Go to wallet" click.
  controller->OnGoToWalletClicked();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return browser()->tab_strip_model()->count() == 2; }));

  // Verify the bubble is hidden.
  EXPECT_FALSE(controller->IsShowingBubble());

  // Switch back to the original tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return browser()->tab_strip_model()->active_index() == 0; }));

  // Verify the bubble is reshown.
  EXPECT_TRUE(controller->IsShowingBubble());
}


}  // namespace
}  // namespace wallet
