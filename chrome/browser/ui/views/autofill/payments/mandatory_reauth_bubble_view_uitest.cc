// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_ui.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_confirmation_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_icon_view.h"
#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_opt_in_bubble_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/test/widget_test.h"

namespace autofill {

class MandatoryReauthBubbleViewUiTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  MandatoryReauthBubbleViewUiTest() {
    if (GetParam()) {
      feature_list_.InitAndEnableFeatureWithParameters(
          ::features::kPageActionsMigration,
          {
              {
                  ::features::kPageActionsMigrationAutofillMandatoryReauth.name,
                  "true",
              },
          });
    }
  }

  ~MandatoryReauthBubbleViewUiTest() override = default;
  MandatoryReauthBubbleViewUiTest(const MandatoryReauthBubbleViewUiTest&) =
      delete;
  MandatoryReauthBubbleViewUiTest& operator=(
      const MandatoryReauthBubbleViewUiTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    MandatoryReauthBubbleControllerImpl::CreateForWebContents(web_contents);
    MandatoryReauthBubbleControllerImpl* controller = GetController();
    DCHECK(controller);
  }

  void ShowBubble() {
    MandatoryReauthBubbleControllerImpl* controller = GetController();
    controller->SetupAndShowBubble(accept_callback.Get(), cancel_callback.Get(),
                                   close_callback.Get());
    views::test::WidgetVisibleWaiter visible_waiter(
        GetOptInBubbleView()->GetWidget());
    visible_waiter.Wait();
  }

  void ReshowBubble() {
    MandatoryReauthBubbleControllerImpl* controller = GetController();
    controller->ReshowBubble();
    if (controller->GetMandatoryReauthBubbleType() ==
        MandatoryReauthBubbleType::kOptIn) {
      views::test::WidgetVisibleWaiter visible_waiter(
          GetOptInBubbleView()->GetWidget());
      visible_waiter.Wait();
    } else {
      views::test::WidgetVisibleWaiter visible_waiter(
          GetConfirmationBubbleView()->GetWidget());
      visible_waiter.Wait();
    }
  }

  bool IsIconVisible() { return GetIconView() && GetIconView()->GetVisible(); }

  MandatoryReauthBubbleControllerImpl* GetController() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }

    return MandatoryReauthBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  views::BubbleDialogDelegate* GetReauthBubble() {
    MandatoryReauthBubbleController* controller = GetController();
    if (!controller) {
      return nullptr;
    }

    if (controller->GetMandatoryReauthBubbleType() ==
        MandatoryReauthBubbleType::kConfirmation) {
      return static_cast<autofill::MandatoryReauthConfirmationBubbleView*>(
          controller->GetBubbleView());
    }

    return static_cast<autofill::MandatoryReauthOptInBubbleView*>(
        controller->GetBubbleView());
  }

  MandatoryReauthOptInBubbleView* GetOptInBubbleView() {
    MandatoryReauthBubbleControllerImpl* controller = GetController();
    return static_cast<MandatoryReauthOptInBubbleView*>(
        controller->GetBubbleView());
  }

  MandatoryReauthConfirmationBubbleView* GetConfirmationBubbleView() {
    MandatoryReauthBubbleControllerImpl* controller = GetController();
    return static_cast<MandatoryReauthConfirmationBubbleView*>(
        controller->GetBubbleView());
  }

  IconLabelBubbleView* GetIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());

    IconLabelBubbleView* icon =
        browser_view->toolbar_button_provider()->GetPageActionView(
            kActionAutofillMandatoryReauth);

    DCHECK(icon);
    return icon;
  }

  void ClickOnView(views::View* view) {
    ui::MouseEvent pressed(ui::EventType::kMousePressed, gfx::Point(),
                           gfx::Point(), ui::EventTimeForNow(),
                           ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMousePressed(pressed);
    ui::MouseEvent released_event =
        ui::MouseEvent(ui::EventType::kMouseReleased, gfx::Point(),
                       gfx::Point(), ui::EventTimeForNow(),
                       ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    view->OnMouseReleased(released_event);
  }

  void ClickOnViewAndWait(
      views::View* view,
      views::BubbleDialogDelegate* mandatory_reauth_bubble) {
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        mandatory_reauth_bubble->GetWidget());
    mandatory_reauth_bubble->ResetViewShownTimeStampForTesting();
    views::BubbleFrameView* bubble_frame_view =
        static_cast<views::BubbleFrameView*>(
            mandatory_reauth_bubble->GetWidget()
                ->non_client_view()
                ->frame_view());
    bubble_frame_view->ResetViewShownTimeStampForTesting();
    ClickOnView(view);
    destroyed_waiter.Wait();
  }

  void ClickOnOkButton(views::BubbleDialogDelegate* mandatory_reauth_bubble) {
    views::View* ok_button = mandatory_reauth_bubble->GetOkButton();
    ClickOnViewAndWait(ok_button, mandatory_reauth_bubble);
  }

  void ClickOnCancelButton(
      views::BubbleDialogDelegate* mandatory_reauth_bubble) {
    views::View* cancel_button = mandatory_reauth_bubble->GetCancelButton();
    ClickOnViewAndWait(cancel_button, mandatory_reauth_bubble);
  }

  void ClickOnCloseButton(
      views::BubbleDialogDelegate* mandatory_reauth_bubble) {
    views::View* close_button =
        mandatory_reauth_bubble->GetBubbleFrameView()->close_button();
    ClickOnViewAndWait(close_button, mandatory_reauth_bubble);
  }

  void ClickOnSettingsLink(
      views::BubbleDialogDelegate* mandatory_reauth_bubble) {
    views::View* settings_link =
        mandatory_reauth_bubble->GetBubbleFrameView()->GetViewByID(
            DialogViewId::SETTINGS_LABEL);
    static_cast<views::StyledLabel*>(settings_link)->ClickFirstLinkForTesting();
  }

  base::MockOnceClosure accept_callback;
  base::MockOnceClosure cancel_callback;
  base::MockRepeatingClosure close_callback;

 protected:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(MandatoryReauthBubbleViewUiTest, ShowBubble) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  EXPECT_TRUE(GetReauthBubble());
  EXPECT_TRUE(IsIconVisible());
  EXPECT_EQ(GetController()->GetMandatoryReauthBubbleType(),
            MandatoryReauthBubbleType::kOptIn);
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.MandatoryReauth.OptInBubbleOffer.FirstShow",
      autofill_metrics::MandatoryReauthOptInBubbleOffer::kShown, 1);
}

IN_PROC_BROWSER_TEST_P(MandatoryReauthBubbleViewUiTest,
                       ClickOptInCancelButton) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  EXPECT_CALL(cancel_callback, Run).Times(1);
  ClickOnCancelButton(GetReauthBubble());
  EXPECT_FALSE(GetReauthBubble());
  EXPECT_FALSE(IsIconVisible());
  EXPECT_EQ(GetController()->GetMandatoryReauthBubbleType(),
            MandatoryReauthBubbleType::kInactive);
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.MandatoryReauth.OptInBubbleResult.FirstShow",
      autofill_metrics::MandatoryReauthOptInBubbleResult::kCancelled, 1);
}

IN_PROC_BROWSER_TEST_P(MandatoryReauthBubbleViewUiTest, ClickOptInOkButton) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  EXPECT_CALL(accept_callback, Run).Times(1);
  ClickOnOkButton(GetReauthBubble());
  EXPECT_FALSE(GetReauthBubble());
  EXPECT_TRUE(IsIconVisible());
  EXPECT_EQ(GetController()->GetMandatoryReauthBubbleType(),
            MandatoryReauthBubbleType::kConfirmation);
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.MandatoryReauth.OptInBubbleResult.FirstShow",
      autofill_metrics::MandatoryReauthOptInBubbleResult::kAccepted, 1);
}

IN_PROC_BROWSER_TEST_P(MandatoryReauthBubbleViewUiTest, ClickOptInCloseButton) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  EXPECT_CALL(close_callback, Run).Times(1);
  ClickOnCloseButton(GetReauthBubble());
  EXPECT_FALSE(GetReauthBubble());
  EXPECT_TRUE(IsIconVisible());
  EXPECT_EQ(GetController()->GetMandatoryReauthBubbleType(),
            MandatoryReauthBubbleType::kOptIn);
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.MandatoryReauth.OptInBubbleResult.FirstShow",
      autofill_metrics::MandatoryReauthOptInBubbleResult::kClosed, 1);
}

IN_PROC_BROWSER_TEST_P(MandatoryReauthBubbleViewUiTest, ReshowOptInBubble) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  ClickOnCloseButton(GetReauthBubble());
  ReshowBubble();
  EXPECT_TRUE(GetReauthBubble());
  EXPECT_TRUE(IsIconVisible());
  EXPECT_EQ(GetController()->GetMandatoryReauthBubbleType(),
            MandatoryReauthBubbleType::kOptIn);
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.MandatoryReauth.OptInBubbleOffer.Reshow",
      autofill_metrics::MandatoryReauthOptInBubbleOffer::kShown, 1);
}

IN_PROC_BROWSER_TEST_P(MandatoryReauthBubbleViewUiTest,
                       ReshowConfirmationBubble) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  ClickOnOkButton(GetReauthBubble());
  ReshowBubble();
  EXPECT_TRUE(GetReauthBubble());
  EXPECT_TRUE(IsIconVisible());
  EXPECT_EQ(GetController()->GetMandatoryReauthBubbleType(),
            MandatoryReauthBubbleType::kConfirmation);
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.MandatoryReauth.OptInConfirmationBubble",
      autofill_metrics::MandatoryReauthOptInConfirmationBubbleMetric::kShown,
      1);
}

IN_PROC_BROWSER_TEST_P(MandatoryReauthBubbleViewUiTest,
                       ClickConfirmationCloseButton) {
  ShowBubble();
  ClickOnOkButton(GetReauthBubble());
  ReshowBubble();
  EXPECT_TRUE(GetReauthBubble());
  EXPECT_EQ(GetController()->GetMandatoryReauthBubbleType(),
            MandatoryReauthBubbleType::kConfirmation);
  // The `close_callback` is only invoked when the opt-in bubble is explicitly
  // closed by the user, so it should not be invoked when it transitions between
  // the opt-in and confirmation states.
  EXPECT_CALL(close_callback, Run).Times(0);
  ClickOnCloseButton(GetReauthBubble());
  EXPECT_FALSE(GetReauthBubble());
  EXPECT_FALSE(IsIconVisible());
  EXPECT_EQ(GetController()->GetMandatoryReauthBubbleType(),
            MandatoryReauthBubbleType::kInactive);
}

IN_PROC_BROWSER_TEST_P(MandatoryReauthBubbleViewUiTest,
                       ClickConfirmationSettingsLink) {
  base::HistogramTester histogram_tester;
  ShowBubble();
  ClickOnOkButton(GetReauthBubble());
  ReshowBubble();
  EXPECT_TRUE(GetReauthBubble());
  EXPECT_EQ(GetController()->GetMandatoryReauthBubbleType(),
            MandatoryReauthBubbleType::kConfirmation);
  EXPECT_CALL(close_callback, Run).Times(0);
  ClickOnSettingsLink(GetReauthBubble());
  histogram_tester.ExpectBucketCount(
      "Autofill.PaymentMethods.MandatoryReauth.OptInConfirmationBubble",
      autofill_metrics::MandatoryReauthOptInConfirmationBubbleMetric::
          kSettingsLinkClicked,
      1);
}

INSTANTIATE_TEST_SUITE_P(All,
                         MandatoryReauthBubbleViewUiTest,
                         ::testing::Bool());

}  // namespace autofill
