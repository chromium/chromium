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
#include "components/autofill/core/common/autofill_features.h"
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

class WalletablePassBubbleControllerBrowserTestBubbleManagerDisabled
    : public WalletablePassBubbleControllerBrowserTest {
 public:
  WalletablePassBubbleControllerBrowserTestBubbleManagerDisabled() {
    scoped_feature_list_.InitAndDisableFeature(
        autofill::features::kAutofillShowBubblesBasedOnPriorities);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the save bubble is reshown when the tab is reactivated after
// clicking "Go to wallet" link.
IN_PROC_BROWSER_TEST_F(WalletablePassBubbleControllerBrowserTest,
                       ReshowSaveBubbleOnTabActivationAfterGoToWallet) {
  auto controller = std::make_unique<WalletablePassSaveBubbleController>(
      browser()->tab_strip_model()->GetTabAtIndex(0));
  wallet::WalletablePass pass;
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

// Tests that the save bubble is not reshown when the tab is reactivated if it
// was hidden by switching tabs.
IN_PROC_BROWSER_TEST_F(
    WalletablePassBubbleControllerBrowserTestBubbleManagerDisabled,
    ShouldNotReshowSaveBubbleOnTabActivation) {
  auto controller = std::make_unique<WalletablePassSaveBubbleController>(
      browser()->tab_strip_model()->GetTabAtIndex(0));
  wallet::WalletablePass pass;
  wallet::LoyaltyCard loyalty_card;
  loyalty_card.plan_name = "Test Plan";
  loyalty_card.issuer_name = "Test Issuer";
  pass.pass_data = std::move(loyalty_card);

  controller->SetUpAndShowSaveBubble(pass, base::DoNothing());

  EXPECT_TRUE(controller->IsShowingBubble());

  // Open a new tab in the foreground.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return browser()->tab_strip_model()->count() == 2; }));

  // Verify the bubble is hidden.
  EXPECT_FALSE(controller->IsShowingBubble());

  // Switch back to the original tab.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return browser()->tab_strip_model()->active_index() == 0; }));

  // Verify the bubble is NOT reshown.
  EXPECT_FALSE(controller->IsShowingBubble());
}

}  // namespace
}  // namespace wallet
