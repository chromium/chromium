// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/wallet_reminder_notice_page_action_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/page_action/test_support/page_action_interactive_test_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class WalletReminderNoticePageActionControllerInteractiveUiTest
    : public PageActionInteractiveTestMixin<InteractiveBrowserTest> {
 public:
  WalletReminderNoticePageActionControllerInteractiveUiTest() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableWalletReminderNotice);
  }
  ~WalletReminderNoticePageActionControllerInteractiveUiTest() override =
      default;

  using PageActionInteractiveTestMixin::WaitForPageActionButtonVisible;

  WalletReminderNoticePageActionController* GetController() {
    tabs::TabInterface* tab = browser()->tab_strip_model()->GetActiveTab();
    return tab ? WalletReminderNoticePageActionController::From(*tab) : nullptr;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    WalletReminderNoticePageActionControllerInteractiveUiTest,
    ShowsAndHidesPageAction) {
  RunTestSequence(
      // Ensure the page action icon is not visible initially.
      WaitForHide(kPageActionWalletReminderNoticeElementId),

      // Trigger `Show()` on the active tab's controller.
      Do([this]() {
        auto* controller = GetController();
        ASSERT_TRUE(controller);
        controller->Show();
      }),

      // Verify the page action icon becomes visible in the LocationBar.
      WaitForPageActionButtonVisible(kActionWalletReminderNotice),

      // Trigger `Hide()` on the active tab's controller.
      Do([this]() {
        auto* controller = GetController();
        ASSERT_TRUE(controller);
        controller->Hide();
      }),

      // Verify the page action icon is hidden.
      WaitForHide(kPageActionWalletReminderNoticeElementId));
}

}  // namespace autofill
