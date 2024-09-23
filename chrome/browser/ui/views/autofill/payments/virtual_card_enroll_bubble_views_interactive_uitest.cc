// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl_test_api.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/dialog_view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_bubble_views.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_enroll_icon_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/payments/test_legal_message_line.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_observer.h"

namespace autofill {
namespace {

constexpr int kCardImageWidthInPx = 32;
constexpr int kCardImageLengthInPx = 20;

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class VirtualCardEnrollBubbleViewsInteractiveUiTest
    : public InProcessBrowserTest {
 public:
  VirtualCardEnrollBubbleViewsInteractiveUiTest() = default;
  ~VirtualCardEnrollBubbleViewsInteractiveUiTest() override = default;
  VirtualCardEnrollBubbleViewsInteractiveUiTest(
      const VirtualCardEnrollBubbleViewsInteractiveUiTest&) = delete;
  VirtualCardEnrollBubbleViewsInteractiveUiTest& operator=(
      const VirtualCardEnrollBubbleViewsInteractiveUiTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    VirtualCardEnrollBubbleControllerImpl* controller =
        static_cast<VirtualCardEnrollBubbleControllerImpl*>(
            VirtualCardEnrollBubbleControllerImpl::GetOrCreate(
                browser()->tab_strip_model()->GetActiveWebContents()));
    DCHECK(controller);
    CreateVirtualCardEnrollmentFields();
  }

  void CreateVirtualCardEnrollmentFields() {
    CreditCard credit_card = test::GetFullServerCard();
    LegalMessageLines google_legal_message = {
        TestLegalMessageLine("google_test_legal_message")};
    LegalMessageLines issuer_legal_message = {
        TestLegalMessageLine("issuer_test_legal_message")};

    upstream_virtual_card_enrollment_fields_.credit_card = credit_card;
    upstream_virtual_card_enrollment_fields_.card_art_image = &card_art_image_;
    upstream_virtual_card_enrollment_fields_.google_legal_message =
        google_legal_message;
    upstream_virtual_card_enrollment_fields_.issuer_legal_message =
        issuer_legal_message;
    upstream_virtual_card_enrollment_fields_.virtual_card_enrollment_source =
        VirtualCardEnrollmentSource::kUpstream;

    downstream_virtual_card_enrollment_fields_ =
        upstream_virtual_card_enrollment_fields_;
    downstream_virtual_card_enrollment_fields_.virtual_card_enrollment_source =
        VirtualCardEnrollmentSource::kDownstream;

    settings_page_virtual_card_enrollment_fields_ =
        upstream_virtual_card_enrollment_fields_;
    settings_page_virtual_card_enrollment_fields_
        .virtual_card_enrollment_source =
        VirtualCardEnrollmentSource::kSettingsPage;
  }

  void ShowBubbleAndWaitUntilShown(
      const VirtualCardEnrollmentFields& virtual_card_enrollment_fields,
      base::OnceClosure accept_virtual_card_callback,
      base::OnceClosure decline_virtual_card_callback) {
    base::RunLoop run_loop;
    base::RepeatingClosure bubble_shown_closure_for_testing_ =
        run_loop.QuitClosure();
    test_api(*GetController())
        .SetBubbleShownClosure(bubble_shown_closure_for_testing_);

    GetController()->ShowBubble(
        virtual_card_enrollment_fields,
        /*accept_virtual_card_callback*/ base::DoNothing(),
        /*decline_virtual_card_callback*/ base::DoNothing());
  }

  void ReshowBubble() { GetController()->ReshowBubble(); }

  bool IsIconVisible() { return GetIconView() && GetIconView()->GetVisible(); }

  bool IsLoadingProgressRowVisible() {
    return GetBubbleViews() &&
           GetBubbleViews()->GetLoadingProgressRowForTesting() &&
           GetBubbleViews()->GetLoadingProgressRowForTesting()->GetVisible();
  }

  VirtualCardEnrollBubbleControllerImpl* GetController() {
    if (!browser() || !browser()->tab_strip_model() ||
        !browser()->tab_strip_model()->GetActiveWebContents()) {
      return nullptr;
    }

    return VirtualCardEnrollBubbleControllerImpl::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  VirtualCardEnrollBubbleViews* GetBubbleViews() {
    VirtualCardEnrollBubbleControllerImpl* controller = GetController();
    if (!controller) {
      return nullptr;
    }

    return static_cast<VirtualCardEnrollBubbleViews*>(
        controller->GetVirtualCardBubbleView());
  }

  void ClickLearnMoreLink() { GetBubbleViews()->LearnMoreLinkClicked(); }

  void ClickGoogleLegalMessageLink() {
    GetBubbleViews()->GoogleLegalMessageClicked(
        autofill::payments::GetVirtualCardEnrollmentSupportUrl());
  }

  void ClickIssuerLegalMessageLink() {
    GetBubbleViews()->IssuerLegalMessageClicked(
        autofill::payments::GetVirtualCardEnrollmentSupportUrl());
  }

  VirtualCardEnrollIconView* GetIconView() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    PageActionIconView* icon =
        browser_view->toolbar_button_provider()->GetPageActionIconView(
            PageActionIconType::kVirtualCardEnroll);
    DCHECK(icon);
    return static_cast<VirtualCardEnrollIconView*>(icon);
  }

  const VirtualCardEnrollmentFields&
  downstream_virtual_card_enrollment_fields() {
    return downstream_virtual_card_enrollment_fields_;
  }

  const VirtualCardEnrollmentFields& upstream_virtual_card_enrollment_fields() {
    return upstream_virtual_card_enrollment_fields_;
  }

  const VirtualCardEnrollmentFields&
  settings_page_virtual_card_enrollment_fields() {
    return settings_page_virtual_card_enrollment_fields_;
  }

  const VirtualCardEnrollmentFields& GetFieldsForSource(
      VirtualCardEnrollmentSource source) {
    switch (source) {
      case VirtualCardEnrollmentSource::kUpstream:
        return upstream_virtual_card_enrollment_fields();
      case VirtualCardEnrollmentSource::kDownstream:
        return downstream_virtual_card_enrollment_fields();
      default:
        return settings_page_virtual_card_enrollment_fields();
    }
  }

  void TestCloseBubbleForExpectedResultFromSource(
      VirtualCardEnrollmentBubbleResult expected_result,
      VirtualCardEnrollmentSource source) {
    base::HistogramTester histogram_tester;
    ShowBubbleAndWaitUntilShown(GetFieldsForSource(source), base::DoNothing(),
                                base::DoNothing());

    ASSERT_TRUE(GetBubbleViews());
    ASSERT_TRUE(IsIconVisible());

    views::Widget::ClosedReason closed_reason;
    switch (expected_result) {
      case VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED:
        closed_reason = views::Widget::ClosedReason::kAcceptButtonClicked;
        break;
      case VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED:
        closed_reason = views::Widget::ClosedReason::kCloseButtonClicked;
        break;
      case VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_LOST_FOCUS:
        closed_reason = views::Widget::ClosedReason::kLostFocus;
        break;
      case VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED:
        closed_reason = views::Widget::ClosedReason::kCancelButtonClicked;
        break;
      case VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_NOT_INTERACTED:
        // The VirtualCardEnrollBubble will be closed differently in the not
        // interacted case, so no need to set |closed_reason| here.
        break;
      case VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_RESULT_UNKNOWN:
        NOTREACHED();
    }

    views::test::WidgetDestroyedWaiter destroyed_waiter(
        GetBubbleViews()->GetWidget());

    if (expected_result != VirtualCardEnrollmentBubbleResult::
                               VIRTUAL_CARD_ENROLLMENT_BUBBLE_NOT_INTERACTED) {
      GetBubbleViews()->GetWidget()->CloseWithReason(closed_reason);
    } else {
      browser()->tab_strip_model()->CloseAllTabs();
    }

    destroyed_waiter.Wait();
    histogram_tester.ExpectBucketCount(
        "Autofill.VirtualCardEnrollBubble.Result." +
            VirtualCardEnrollmentSourceToMetricSuffix(source) + ".FirstShow",
        expected_result, 1);
  }

  void CloseBubbleForReasonAndWaitTillDestroyed(
      views::Widget::ClosedReason reason) {
    ASSERT_TRUE(GetBubbleViews());
    views::test::WidgetDestroyedWaiter destroyed_waiter(
        GetBubbleViews()->GetWidget());
    GetBubbleViews()->GetWidget()->CloseWithReason(reason);
    destroyed_waiter.Wait();
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  VirtualCardEnrollmentFields downstream_virtual_card_enrollment_fields_;
  VirtualCardEnrollmentFields upstream_virtual_card_enrollment_fields_;
  VirtualCardEnrollmentFields settings_page_virtual_card_enrollment_fields_;
  gfx::ImageSkia card_art_image_ =
      gfx::test::CreateImage(kCardImageWidthInPx, kCardImageLengthInPx)
          .AsImageSkia();
};

class VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized
    : public VirtualCardEnrollBubbleViewsInteractiveUiTest,
      public testing::WithParamInterface<VirtualCardEnrollmentSource> {
 public:
  VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized() {
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  }
  ~VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized() override =
      default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    testing::Values(VirtualCardEnrollmentSource::kUpstream,
                    VirtualCardEnrollmentSource::kDownstream,
                    VirtualCardEnrollmentSource::kSettingsPage));

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    ShowBubble) {
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  ShowBubbleAndWaitUntilShown(
      GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
      base::DoNothing());

  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsIconVisible());
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    Metrics_BubbleLostFocus) {
  TestCloseBubbleForExpectedResultFromSource(
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_LOST_FOCUS,
      GetParam());
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    Metrics_BubbleCancelled) {
  TestCloseBubbleForExpectedResultFromSource(
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED,
      GetParam());
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    Metrics_BubbleClosed) {
  TestCloseBubbleForExpectedResultFromSource(
      VirtualCardEnrollmentBubbleResult::VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED,
      GetParam());
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    Metrics_NotInteracted) {
  TestCloseBubbleForExpectedResultFromSource(
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_NOT_INTERACTED,
      GetParam());
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    ShownAndLostFocusTest_AllSources) {
  base::HistogramTester histogram_tester;
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  ShowBubbleAndWaitUntilShown(
      GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
      base::DoNothing());

  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Shown." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source),
      false, 1);

  // Simulates deactivation due to clicking the close button.
  CloseBubbleForReasonAndWaitTillDestroyed(
      views::Widget::ClosedReason::kCloseButtonClicked);

  // Confirms .FirstShow metrics.
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Result." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".FirstShow",
      VirtualCardEnrollmentBubbleResult::VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED,
      1);

  // Bubble is reshown by the user.
  ReshowBubble();

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Shown." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source),
      true, 1);

  // Simulates deactivation due to clicking the close button.
  CloseBubbleForReasonAndWaitTillDestroyed(
      views::Widget::ClosedReason::kCloseButtonClicked);

  // Confirms .Reshows metrics.
  histogram_tester.ExpectUniqueSample(
      "Autofill.VirtualCardEnrollBubble.Result." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".Reshows",
      VirtualCardEnrollmentBubbleResult::VIRTUAL_CARD_ENROLLMENT_BUBBLE_CLOSED,
      1);

  // Bubble is reshown by the user. Closing a reshown bubble makes the
  // browser inactive for some reason, so we must reactivate it first.
  browser()->window()->Activate();
  ReshowBubble();

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Shown." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source),
      true, 2);
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    LearnMoreTest_AllSources) {
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  base::HistogramTester histogram_tester;
  ShowBubbleAndWaitUntilShown(
      GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
      base::DoNothing());

  ASSERT_TRUE(GetBubbleViews());
  ClickLearnMoreLink();

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnroll.LinkClicked." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".LearnMoreLink",
      true, 1);
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    GoogleLegalMessageTest_AllSources) {
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  base::HistogramTester histogram_tester;
  ShowBubbleAndWaitUntilShown(
      GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
      base::DoNothing());

  ASSERT_TRUE(GetBubbleViews());
  ClickGoogleLegalMessageLink();

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnroll.LinkClicked." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".GoogleLegalMessageLink",
      true, 1);
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    IssuerLegalMessageTest_AllSources) {
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  base::HistogramTester histogram_tester;
  ShowBubbleAndWaitUntilShown(
      GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
      base::DoNothing());

  ASSERT_TRUE(GetBubbleViews());
  ClickIssuerLegalMessageLink();

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnroll.LinkClicked." +
          VirtualCardEnrollmentSourceToMetricSuffix(GetParam()) +
          ".IssuerLegalMessageLink",
      true, 1);
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    CardArtAvailableTest_AllSources) {
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  base::HistogramTester histogram_tester;
  ShowBubbleAndWaitUntilShown(
      GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
      base::DoNothing());

  ASSERT_TRUE(GetBubbleViews());

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnroll.CardArtImageAvailable." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source),
      true, 1);
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    CardArtNotAvailableTest_AllSources) {
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  base::HistogramTester histogram_tester;
  VirtualCardEnrollmentFields fields =
      GetFieldsForSource(virtual_card_enrollment_source);
  fields.card_art_image = nullptr;
  ShowBubbleAndWaitUntilShown(fields, base::DoNothing(), base::DoNothing());

  ASSERT_TRUE(GetBubbleViews());

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnroll.CardArtImageAvailable." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source),
      false, 1);
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    PreviouslyDeclinedTest_AllSources) {
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  base::HistogramTester histogram_tester;
  VirtualCardEnrollmentFields fields =
      GetFieldsForSource(virtual_card_enrollment_source);

  // Simulates that the enroll bubble is shown for the first time for the card,
  // and verifies the logging.
  fields.previously_declined = false;

  ShowBubbleAndWaitUntilShown(fields, base::DoNothing(), base::DoNothing());
  ASSERT_TRUE(GetBubbleViews());

  CloseBubbleForReasonAndWaitTillDestroyed(
      views::Widget::ClosedReason::kCancelButtonClicked);

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Result." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".FirstShow.WithNoPreviousStrike",
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED,
      1);

  // Simulates that the bubble has been declined before, and verifies the
  // logging.
  fields.previously_declined = true;

  ShowBubbleAndWaitUntilShown(fields, base::DoNothing(), base::DoNothing());
  ASSERT_TRUE(GetBubbleViews());

  CloseBubbleForReasonAndWaitTillDestroyed(
      views::Widget::ClosedReason::kCancelButtonClicked);

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Result." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".FirstShow.WithPreviousStrikes",
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_CANCELLED,
      1);
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    NoLoggingOnLinkClickReshowBubbleTest) {
  base::HistogramTester histogram_tester;
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  ShowBubbleAndWaitUntilShown(
      GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
      base::DoNothing());

  EXPECT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsIconVisible());

  // Verify shown metrics: first show
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Shown." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source),
      false, 1);

  ClickLearnMoreLink();

  // Verify link click metrics: clicked
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnroll.LinkClicked." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".LearnMoreLink",
      true, 1);

  // Switch back to the tab containing the bubble
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Verify close metrics: never closed
  histogram_tester.ExpectTotalCount(
      "Autofill.VirtualCardEnrollBubble.Result." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".FirstShow",
      0);

  // Verify show and reshow metrics
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.VirtualCardEnrollBubble.Shown." +
                                     VirtualCardEnrollmentSourceToMetricSuffix(
                                         virtual_card_enrollment_source)),
      BucketsAre(base::Bucket(false, 1), base::Bucket(true, 0)));
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    IconViewAccessibleName) {
  EXPECT_EQ(GetIconView()->GetViewAccessibility().GetCachedName(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_FALLBACK_ICON_TOOLTIP));
  EXPECT_EQ(GetIconView()->GetTextForTooltipAndAccessibleName(),
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_VIRTUAL_CARD_ENROLLMENT_FALLBACK_ICON_TOOLTIP));
}

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestParameterized,
    ShowLoadingViewOnAccept) {
  base::HistogramTester histogram_tester;
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  ShowBubbleAndWaitUntilShown(
      GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
      base::DoNothing());
  ASSERT_TRUE(GetBubbleViews());
  EXPECT_FALSE(IsLoadingProgressRowVisible());
  EXPECT_EQ(GetBubbleViews()->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel));

  GetBubbleViews()->AcceptDialog();

  ASSERT_TRUE(GetBubbleViews());
  EXPECT_TRUE(IsLoadingProgressRowVisible());
  views::View* loading_throbber =
      GetBubbleViews()->GetViewByID(DialogViewId::LOADING_THROBBER);
  EXPECT_TRUE(loading_throbber->IsDrawn());
  EXPECT_EQ(GetBubbleViews()->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kNone));

  CloseBubbleForReasonAndWaitTillDestroyed(
      views::Widget::ClosedReason::kAcceptButtonClicked);

  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Result." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".FirstShow",
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED,
      1);
}

class
    VirtualCardEnrollBubbleViewsInteractiveUiTestDisabledLoadingAndConfirmation
    : public VirtualCardEnrollBubbleViewsInteractiveUiTest,
      public testing::WithParamInterface<VirtualCardEnrollmentSource> {
 public:
  VirtualCardEnrollBubbleViewsInteractiveUiTestDisabledLoadingAndConfirmation() {
    feature_list_.InitAndDisableFeature(
        features::kAutofillEnableVcnEnrollLoadingAndConfirmation);
  }
  ~VirtualCardEnrollBubbleViewsInteractiveUiTestDisabledLoadingAndConfirmation()
      override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    VirtualCardEnrollBubbleViewsInteractiveUiTestDisabledLoadingAndConfirmation,
    testing::Values(VirtualCardEnrollmentSource::kUpstream,
                    VirtualCardEnrollmentSource::kDownstream,
                    VirtualCardEnrollmentSource::kSettingsPage));

IN_PROC_BROWSER_TEST_P(
    VirtualCardEnrollBubbleViewsInteractiveUiTestDisabledLoadingAndConfirmation,
    CloseBubbleOnAcceptWhenLoadingAndConfirmationIsDisabled) {
  base::HistogramTester histogram_tester;
  VirtualCardEnrollmentSource virtual_card_enrollment_source = GetParam();
  ShowBubbleAndWaitUntilShown(
      GetFieldsForSource(virtual_card_enrollment_source), base::DoNothing(),
      base::DoNothing());
  ASSERT_TRUE(GetBubbleViews());
  EXPECT_FALSE(IsLoadingProgressRowVisible());
  EXPECT_EQ(GetBubbleViews()->buttons(),
            static_cast<int>(ui::mojom::DialogButton::kOk) |
                static_cast<int>(ui::mojom::DialogButton::kCancel));

  views::test::WidgetDestroyedWaiter destroyed_waiter(
      GetBubbleViews()->GetWidget());
  GetBubbleViews()->AcceptDialog();
  destroyed_waiter.Wait();

  EXPECT_FALSE(GetBubbleViews());
  EXPECT_FALSE(IsIconVisible());
  histogram_tester.ExpectBucketCount(
      "Autofill.VirtualCardEnrollBubble.Result." +
          VirtualCardEnrollmentSourceToMetricSuffix(
              virtual_card_enrollment_source) +
          ".FirstShow",
      VirtualCardEnrollmentBubbleResult::
          VIRTUAL_CARD_ENROLLMENT_BUBBLE_ACCEPTED,
      1);
}

}  // namespace autofill
