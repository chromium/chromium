// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_and_virtual_card_enroll_confirmation_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/test/widget_test.h"

namespace autofill {

class SaveCardConfirmationBubbleViewsInteractiveUiTest
    : public InProcessBrowserTest {
 public:
  SaveCardConfirmationBubbleViewsInteractiveUiTest() = default;
  ~SaveCardConfirmationBubbleViewsInteractiveUiTest() override = default;
  SaveCardConfirmationBubbleViewsInteractiveUiTest(
      const SaveCardConfirmationBubbleViewsInteractiveUiTest&) = delete;
  SaveCardConfirmationBubbleViewsInteractiveUiTest& operator=(
      const SaveCardConfirmationBubbleViewsInteractiveUiTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    SaveCardBubbleControllerImpl* save_card_controller =
        static_cast<SaveCardBubbleControllerImpl*>(
            SaveCardBubbleControllerImpl::GetOrCreate(
                browser()->tab_strip_model()->GetActiveWebContents()));
    CHECK(save_card_controller);
  }

  SaveCardBubbleControllerImpl* Controller() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }

    return SaveCardBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  SaveCardAndVirtualCardEnrollConfirmationBubbleViews* BubbleView() {
    return static_cast<SaveCardAndVirtualCardEnrollConfirmationBubbleViews*>(
        Controller()->GetPaymentBubbleView());
  }

  SavePaymentIconView* IconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kSaveCard);
    CHECK(icon);
    return static_cast<SavePaymentIconView*>(icon);
  }

  void ShowBubble(bool card_saved) {
    Controller()->ShowConfirmationBubbleView(card_saved);
  }

  void HideBubble(views::Widget::ClosedReason closed_reason) {
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        BubbleView()->GetWidget());
    BubbleView()->GetWidget()->CloseWithReason(closed_reason);
    destroyed_waiter.Wait();
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillEnableSaveCardLoadingAndConfirmation};
};

IN_PROC_BROWSER_TEST_F(SaveCardConfirmationBubbleViewsInteractiveUiTest,
                       ShowSuccessBubbleViewThenHideBubbleView) {
  ShowBubble(/*card_saved=*/true);
  EXPECT_NE(BubbleView(), nullptr);
  EXPECT_TRUE(IconView()->GetVisible());

  SaveCardAndVirtualCardEnrollConfirmationUiParams ui_params =
      BubbleView()->ui_params_;
  EXPECT_TRUE(ui_params.is_success);
  EXPECT_EQ(ui_params.title_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_TITLE_TEXT));
  EXPECT_EQ(ui_params.description_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
  EXPECT_TRUE(ui_params.failure_button_text.empty());

  HideBubble(views::Widget::ClosedReason::kLostFocus);
  EXPECT_EQ(BubbleView(), nullptr);
  EXPECT_FALSE(IconView()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SaveCardConfirmationBubbleViewsInteractiveUiTest,
                       ShowFailureBubbleViewThenHideBubbleView) {
  ShowBubble(/*card_saved=*/false);
  EXPECT_NE(BubbleView(), nullptr);
  EXPECT_TRUE(IconView()->GetVisible());

  SaveCardAndVirtualCardEnrollConfirmationUiParams ui_params =
      BubbleView()->ui_params_;
  EXPECT_FALSE(ui_params.is_success);
  EXPECT_EQ(ui_params.title_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_TITLE_TEXT));
  EXPECT_EQ(ui_params.description_text,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_DESCRIPTION_TEXT));
  EXPECT_EQ(
      ui_params.failure_button_text,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_BUTTON_TEXT));

  HideBubble(views::Widget::ClosedReason::kLostFocus);
  EXPECT_EQ(BubbleView(), nullptr);
  EXPECT_FALSE(IconView()->GetVisible());
}

}  // namespace autofill
