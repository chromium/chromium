// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/wallet_reminder_notice_bubble_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_bubble_controller.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "ui/actions/actions.h"

namespace autofill {

class WalletReminderNoticeBubbleViewBrowserTest : public InProcessBrowserTest {
 protected:
  WalletReminderNoticeBubbleViewBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableWalletReminderNotice);
  }
  ~WalletReminderNoticeBubbleViewBrowserTest() override = default;
  WalletReminderNoticeBubbleViewBrowserTest(
      const WalletReminderNoticeBubbleViewBrowserTest&) = delete;
  WalletReminderNoticeBubbleViewBrowserTest& operator=(
      const WalletReminderNoticeBubbleViewBrowserTest&) = delete;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(WalletReminderNoticeBubbleViewBrowserTest, ShowBubble) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  auto* controller = WalletReminderNoticeBubbleController::From(*tab);
  ASSERT_TRUE(controller);

  EXPECT_EQ(controller->GetBubbleView(), nullptr);

  controller->QueueOrShowBubble();

  EXPECT_NE(controller->GetBubbleView(), nullptr);
}

IN_PROC_BROWSER_TEST_F(WalletReminderNoticeBubbleViewBrowserTest,
                       ActionItemUpdatedWithBubbleVisibility) {
  tabs::TabInterface* tab = browser()->GetActiveTabInterface();
  ASSERT_TRUE(tab);

  auto* controller = WalletReminderNoticeBubbleController::From(*tab);
  ASSERT_TRUE(controller);

  actions::ActionItem* action = actions::ActionManager::Get().FindAction(
      kActionWalletReminderNotice,
      BrowserActions::From(browser())->root_action_item());
  ASSERT_NE(action, nullptr);
  EXPECT_FALSE(action->GetIsShowingBubble());

  // Show bubble.
  controller->QueueOrShowBubble();

  auto* bubble_view = controller->GetBubbleView();
  ASSERT_NE(bubble_view, nullptr);
  EXPECT_TRUE(action->GetIsShowingBubble());

  // Close bubble.
  bubble_view->Hide();
  EXPECT_FALSE(action->GetIsShowingBubble());
}

}  // namespace autofill
