// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/save_card_bubble_controller_impl.h"

#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl_test_api.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_and_virtual_card_enroll_confirmation_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_card_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/save_payment_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_icon_view.h"
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

  SaveCardBubbleControllerImpl* GetController() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }

    return SaveCardBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  SaveCardAndVirtualCardEnrollConfirmationBubbleViews* BubbleView() {
    return static_cast<SaveCardAndVirtualCardEnrollConfirmationBubbleViews*>(
        GetController()->GetPaymentBubbleView());
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
    GetController()->ShowConfirmationBubbleView(card_saved);
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

  EXPECT_TRUE(BubbleView()->ShouldShowCloseButton());
  EXPECT_TRUE(BubbleView()
                  ->GetBubbleFrameView()
                  ->GetHeaderViewForTesting()
                  ->GetVisible());
  EXPECT_NE(BubbleView()->GetBubbleFrameView()->title(), nullptr);
  EXPECT_EQ(BubbleView()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_TITLE_TEXT));
  EXPECT_TRUE(
      BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL)->GetVisible());
  EXPECT_EQ(static_cast<views::Label*>(
                BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL))
                ->GetText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
  EXPECT_EQ(BubbleView()->GetDialogButtons(), ui::DIALOG_BUTTON_NONE);
  EXPECT_TRUE(IconView()->GetVisible());

  HideBubble(views::Widget::ClosedReason::kLostFocus);
  EXPECT_EQ(BubbleView(), nullptr);
  EXPECT_FALSE(IconView()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(SaveCardConfirmationBubbleViewsInteractiveUiTest,
                       ShowFailureBubbleViewThenHideBubbleView) {
  ShowBubble(/*card_saved=*/false);

  EXPECT_NE(BubbleView(), nullptr);
  EXPECT_FALSE(BubbleView()->ShouldShowCloseButton());
  EXPECT_EQ(BubbleView()->GetBubbleFrameView()->GetHeaderViewForTesting(),
            nullptr);
  EXPECT_NE(BubbleView()->GetBubbleFrameView()->title(), nullptr);
  EXPECT_EQ(BubbleView()->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_TITLE_TEXT));
  EXPECT_TRUE(
      BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL)->GetVisible());
  EXPECT_EQ(static_cast<views::Label*>(
                BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL))
                ->GetText(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_CARD_CONFIRMATION_FAILURE_DESCRIPTION_TEXT));
  EXPECT_EQ(BubbleView()->GetDialogButtons(), ui::DIALOG_BUTTON_OK);
  EXPECT_EQ(
      BubbleView()->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_BUTTON_TEXT));
  EXPECT_TRUE(IconView()->GetVisible());

  HideBubble(views::Widget::ClosedReason::kLostFocus);
  EXPECT_EQ(BubbleView(), nullptr);
  EXPECT_FALSE(IconView()->GetVisible());
}

class VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest
    : public InProcessBrowserTest {
 public:
  VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest() = default;
  ~VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest() override =
      default;
  VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest(
      const VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest&) =
      delete;
  VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest& operator=(
      const VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest&) =
      delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    VirtualCardEnrollBubbleControllerImpl* virtual_card_enroll_controller =
        static_cast<VirtualCardEnrollBubbleControllerImpl*>(
            VirtualCardEnrollBubbleControllerImpl::GetOrCreate(
                browser()->tab_strip_model()->GetActiveWebContents()));
    CHECK(virtual_card_enroll_controller);
  }

  VirtualCardEnrollBubbleControllerImpl* GetController() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }

    return VirtualCardEnrollBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  SaveCardAndVirtualCardEnrollConfirmationBubbleViews* BubbleView() {
    return static_cast<SaveCardAndVirtualCardEnrollConfirmationBubbleViews*>(
        GetController()->GetVirtualCardBubbleView());
  }

  VirtualCardEnrollIconView* IconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kVirtualCardEnroll);
    CHECK(icon);
    return static_cast<VirtualCardEnrollIconView*>(icon);
  }

  void ShowBubble(bool is_vcn_enrolled) {
    GetController()->ShowConfirmationBubbleView(is_vcn_enrolled);
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillEnableVcnEnrollLoadingAndConfirmation};
};

IN_PROC_BROWSER_TEST_F(
    VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest,
    ShowSuccessBubbleViewThenHideBubbleView) {
  ShowBubble(/*is_vcn_enrolled=*/true);

  EXPECT_NE(BubbleView(), nullptr);
  EXPECT_TRUE(BubbleView()->ShouldShowCloseButton());
  EXPECT_TRUE(BubbleView()
                  ->GetBubbleFrameView()
                  ->GetHeaderViewForTesting()
                  ->GetVisible());
  EXPECT_NE(BubbleView()->GetBubbleFrameView()->title(), nullptr);
  EXPECT_EQ(
      BubbleView()->GetWindowTitle(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_TITLE_TEXT));
  EXPECT_TRUE(
      BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL)->GetVisible());
  EXPECT_EQ(
      static_cast<views::Label*>(
          BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL))
          ->GetText(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_SUCCESS_DESCRIPTION_TEXT));
  EXPECT_EQ(BubbleView()->GetDialogButtons(), ui::DIALOG_BUTTON_NONE);
  EXPECT_TRUE(IconView()->GetVisible());

  GetController()->HideIconAndBubble();
  EXPECT_EQ(BubbleView(), nullptr);
  EXPECT_FALSE(IconView()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(
    VirtualCardEnrollConfirmationBubbleViewsInteractiveUiTest,
    ShowFailureBubbleViewThenHideBubbleView) {
  CreditCard card = test::GetCreditCard();
  VirtualCardEnrollmentFields enrollment_fields;
  enrollment_fields.credit_card = card;
  test_api(GetController()).SetFields(enrollment_fields);
  ShowBubble(/*is_vcn_enrolled=*/false);

  EXPECT_NE(BubbleView(), nullptr);
  EXPECT_FALSE(BubbleView()->ShouldShowCloseButton());
  EXPECT_EQ(BubbleView()->GetBubbleFrameView()->GetHeaderViewForTesting(),
            nullptr);
  EXPECT_NE(BubbleView()->GetBubbleFrameView()->title(), nullptr);
  EXPECT_EQ(
      BubbleView()->GetWindowTitle(),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_TITLE_TEXT));
  EXPECT_TRUE(
      BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL)->GetVisible());
  EXPECT_EQ(
      static_cast<views::Label*>(
          BubbleView()->GetViewByID(DialogViewId::DESCRIPTION_LABEL))
          ->GetText(),
      l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_DESCRIPTION_TEXT,
          card.NetworkAndLastFourDigits()));
  EXPECT_EQ(BubbleView()->GetDialogButtons(), ui::DIALOG_BUTTON_OK);
  EXPECT_EQ(
      BubbleView()->GetDialogButtonLabel(ui::DIALOG_BUTTON_OK),
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_CARD_AND_VIRTUAL_CARD_ENROLL_CONFIRMATION_FAILURE_BUTTON_TEXT));
  EXPECT_TRUE(IconView()->GetVisible());

  GetController()->HideIconAndBubble();
  EXPECT_EQ(BubbleView(), nullptr);
  EXPECT_FALSE(IconView()->GetVisible());
}

}  // namespace autofill
