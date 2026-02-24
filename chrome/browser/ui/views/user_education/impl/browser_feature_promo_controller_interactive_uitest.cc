// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/toolbar_controller_util.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/impl/browser_feature_promo_preconditions.h"
#include "chrome/browser/ui/views/user_education/impl/browser_user_education_interface_impl.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_handle.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/test/test_custom_help_bubble_view.h"
#include "components/user_education/views/custom_help_bubble_view.h"
#include "components/user_education/views/help_bubble_view.h"
#include "components/user_education/views/help_bubble_views.h"
#include "content/public/test/browser_test.h"
#include "omnibox_event.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/events/event_modifiers.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/widget_focus_observer.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Ref;
using ::testing::Return;

using user_education::EndFeaturePromoReason;
using user_education::FeaturePromoClosedReason;
using user_education::FeaturePromoRegistry;
using user_education::FeaturePromoSpecification;

namespace {
BASE_FEATURE(kToastTestFeature,
             "ToastTestFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCustomActionTestFeature,
             "CustomActionTestFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kLegalNoticeTestFeature,
             "LegalNoticeTestFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kCustomUiTestFeature,
             "TEST_CustomUiTestFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

class BrowserFeaturePromoControllerUiTestBase
    : public InteractiveFeaturePromoTest {
 public:
  explicit BrowserFeaturePromoControllerUiTestBase(
      ClockMode clock_mode = ClockMode::kUseDefaultClock)
      : InteractiveFeaturePromoTest(UseMockTracker(), clock_mode) {}
  ~BrowserFeaturePromoControllerUiTestBase() override = default;

  void OnCustomUiCustomAction(
      const user_education::UserEducationContextPtr& context,
      user_education::FeaturePromoHandle promo_handle) {
    EXPECT_EQ(private_test_impl().default_context(),
              context->GetElementContext());
    continued_promo_handle_ = std::move(promo_handle);
  }

  // Verifies that `CanShowPromo()` returns `expected_result`.
  auto QueryIPH(const base::Feature& iph_feature,
                user_education::FeaturePromoResult expected_result) {
    std::ostringstream oss;
    oss << "QueryIPH(" << iph_feature.name << ", " << expected_result << ")";
    return CheckResult(
        [this, &iph_feature]() {
          return promo_controller()->CanShowPromo(iph_feature,
                                                  user_education_context());
        },
        expected_result, oss.str());
  }

  auto UseFeature(const base::Feature& iph_feature) {
    return Steps(Do(base::BindLambdaForTesting([this, &iph_feature]() {
      EXPECT_TRUE(promo_controller()->IsPromoActive(iph_feature));
      promo_controller()->EndPromo(iph_feature,
                                   EndFeaturePromoReason::kFeatureEngaged);

      EXPECT_FALSE(promo_controller()->IsPromoActive(iph_feature));
    })));
  }

  struct ExpectedMetrics {
    int dismiss_count = 0;
    int snooze_count = 0;
    int cancel_count = 0;
    int abort_count = 0;
    int feature_engaged_count = 0;
    int custom_action_count = 0;
  };

  auto CheckMetrics(const base::Feature& iph_feature,
                    ExpectedMetrics expected) {
    return Check(
        base::BindLambdaForTesting([this, &iph_feature, expected]() {
          EXPECT_EQ(expected.dismiss_count,
                    user_action_tester_.GetActionCount(
                        std::string("UserEducation.MessageAction.Dismiss.")
                            .append(iph_feature.name)));
          EXPECT_EQ(expected.snooze_count,
                    user_action_tester_.GetActionCount(
                        std::string("UserEducation.MessageAction.Snooze.")
                            .append(iph_feature.name)));
          EXPECT_EQ(expected.cancel_count,
                    user_action_tester_.GetActionCount(
                        std::string("UserEducation.MessageAction.Cancel.")
                            .append(iph_feature.name)));
          EXPECT_EQ(
              expected.abort_count,
              user_action_tester_.GetActionCount(
                  std::string("UserEducation.MessageAction.AbortedByFeature.")
                      .append(iph_feature.name)));
          EXPECT_EQ(
              expected.feature_engaged_count,
              user_action_tester_.GetActionCount(
                  std::string("UserEducation.MessageAction.FeatureEngaged.")
                      .append(iph_feature.name)));
          EXPECT_EQ(expected.custom_action_count,
                    user_action_tester_.GetActionCount(
                        std::string("UserEducation.MessageAction.Action.")
                            .append(iph_feature.name)));

          std::string action_name = "UserEducation.MessageAction.";
          action_name.append(iph_feature.name);

          histogram_tester_.ExpectBucketCount(
              action_name, static_cast<int>(FeaturePromoClosedReason::kDismiss),
              expected.dismiss_count);
          histogram_tester_.ExpectBucketCount(
              action_name, static_cast<int>(FeaturePromoClosedReason::kSnooze),
              expected.snooze_count);
          histogram_tester_.ExpectBucketCount(
              action_name, static_cast<int>(FeaturePromoClosedReason::kCancel),
              expected.cancel_count);
          histogram_tester_.ExpectBucketCount(
              action_name,
              static_cast<int>(FeaturePromoClosedReason::kAbortedByFeature),
              expected.abort_count);
          histogram_tester_.ExpectBucketCount(
              action_name,
              static_cast<int>(FeaturePromoClosedReason::kFeatureEngaged),
              expected.feature_engaged_count);
          histogram_tester_.ExpectBucketCount(
              action_name, static_cast<int>(FeaturePromoClosedReason::kAction),
              expected.custom_action_count);

          return !testing::Test::HasNonfatalFailure();
        }),
        "Metrics");
  }

  auto PressEscAndWaitForClose(ElementSpecifier spec) {
    auto widget =
        base::MakeRefCounted<base::RefCountedData<const views::Widget*>>(
            nullptr);
    return Steps(
        WaitForShow(spec),
        IfView(
            spec,
            [widget](const views::View* view) {
              widget.get()->data = view->GetWidget();
              return !view->GetWidget()->IsActive();
            },
            Then(ObserveState(views::test::kCurrentWidgetFocus),
                 WaitForState(views::test::kCurrentWidgetFocus,
                              [widget]() { return widget.get()->data; }))),
        SendAccelerator(spec,
                        ui::Accelerator(ui::VKEY_ESCAPE, ui::MODIFIER_NONE)),
        WaitForHide(spec));
  }

  user_education::FeaturePromoController* promo_controller() const {
    return UserEducationServiceFactory::GetForBrowserContext(
               browser()->profile())
        ->GetFeaturePromoControllerForTesting();
  }

  const user_education::UserEducationContextPtr& user_education_context()
      const {
    return BrowserUserEducationInterface::From(browser())
        ->GetUserEducationContextForTesting();
  }

 protected:
  base::UserActionTester user_action_tester_;
  base::HistogramTester histogram_tester_;
  user_education::FeaturePromoHandle continued_promo_handle_;
};

class BrowserFeaturePromoControllerUiTest
    : public BrowserFeaturePromoControllerUiTestBase {
 public:
  BrowserFeaturePromoControllerUiTest() = default;
  ~BrowserFeaturePromoControllerUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();

    // Register test features.
    RegisterTestFeature(
        browser(),
        user_education::FeaturePromoSpecification::CreateForToastPromo(
            kToastTestFeature, kToolbarAppMenuButtonElementId,
            IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE, IDS_TUTORIAL_TAB_GROUP_COLLAPSE,
            FeaturePromoSpecification::AcceleratorInfo()));
    RegisterTestFeature(
        browser(),
        std::move(
            user_education::FeaturePromoSpecification::CreateForCustomAction(
                kCustomActionTestFeature, kToolbarAppMenuButtonElementId,
                IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE,
                IDS_TUTORIAL_TAB_GROUP_COLLAPSE, custom_action_callback_.Get())
                .SetInAnyContext(true)));

    RegisterTestFeature(
        browser(),
        std::move(
            user_education::FeaturePromoSpecification::CreateForCustomAction(
                kLegalNoticeTestFeature, kToolbarAppMenuButtonElementId,
                IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE,
                IDS_TUTORIAL_TAB_GROUP_COLLAPSE, base::DoNothing())
                .set_promo_subtype_for_testing(
                    user_education::FeaturePromoSpecification::PromoSubtype::
                        kLegalNotice)));
    RegisterTestFeature(
        browser(),
        user_education::FeaturePromoSpecification::CreateForCustomUi(
            kCustomUiTestFeature, kToolbarAppMenuButtonElementId,
            user_education::CreateCustomHelpBubbleViewFactoryCallback(
                base::BindRepeating(
                    [](const user_education::UserEducationContextPtr&
                           reference_context,
                       FeaturePromoSpecification::BuildHelpBubbleParams
                           build_params) {
                      auto* const anchor_element =
                          build_params.anchor_element.get();
                      return std::make_unique<
                          user_education::test::TestCustomHelpBubbleView>(
                          anchor_element->AsA<views::TrackedElementViews>()
                              ->view(),
                          user_education::HelpBubbleViews::TranslateArrow(
                              build_params.arrow));
                    })),
            base::BindRepeating(&BrowserFeaturePromoControllerUiTestBase::
                                    OnCustomUiCustomAction,
                                weak_ptr_factory_.GetWeakPtr())));
  }

  base::MockCallback<FeaturePromoSpecification::CustomActionCallback>
      custom_action_callback_;

 private:
  base::WeakPtrFactory<BrowserFeaturePromoControllerUiTest> weak_ptr_factory_{
      this};
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest, LogsAbortMetrics) {
  RunTestSequence(
      MaybeShowPromo(kToastTestFeature), AbortPromo(kToastTestFeature),
      CheckMetrics(kToastTestFeature, ExpectedMetrics{.abort_count = 1}));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       LogsEngagedMetrics) {
  RunTestSequence(MaybeShowPromo(kToastTestFeature),
                  UseFeature(kToastTestFeature),
                  CheckMetrics(kToastTestFeature,
                               ExpectedMetrics{.feature_engaged_count = 1}));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       LogsCustomActionMetrics) {
  EXPECT_CALL(custom_action_callback_, Run).Times(1);
  RunTestSequence(MaybeShowPromo(kCustomActionTestFeature),
                  PressNonDefaultPromoButton(),
                  CheckMetrics(kCustomActionTestFeature,
                               ExpectedMetrics{.custom_action_count = 1}));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       LogsCancelOnEscPressed) {
  RunTestSequence(
      MaybeShowPromo(kCustomActionTestFeature),
      PressEscAndWaitForClose(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckMetrics(kCustomActionTestFeature,
                   ExpectedMetrics{.cancel_count = 1}));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CanShowPromoReturnsExpectedValue) {
  RunTestSequence(
      QueryIPH(kToastTestFeature,
               user_education::FeaturePromoResult::Success()),
      MaybeShowPromo(kToastTestFeature),
      QueryIPH(kToastTestFeature,
               user_education::FeaturePromoResult::kBlockedByPromo),
      PressClosePromoButton(),
      QueryIPH(kToastTestFeature,
               user_education::FeaturePromoResult::kPermanentlyDismissed));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CallbackHappensAfterCancel) {
  bool called = false;
  FeaturePromoClosedReason close_reason = FeaturePromoClosedReason::kAbortPromo;

  EXPECT_CALL(custom_action_callback_, Run).Times(0);

  user_education::FeaturePromoParams params(kCustomActionTestFeature);
  params.close_callback =
      base::BindLambdaForTesting([this, &called, &close_reason]() {
        called = true;
        EXPECT_TRUE(promo_controller()->HasPromoBeenDismissed(
            user_education::FeaturePromoParams(kCustomActionTestFeature),
            &close_reason));
      });

  RunTestSequence(
      MaybeShowPromo(std::move(params),
                     user_education::FeaturePromoResult::Success()),
      PressClosePromoButton(), CheckVariable(called, true),
      CheckVariable(close_reason, FeaturePromoClosedReason::kCancel));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CallbackHappensAfterConfirm) {
  bool called = false;
  FeaturePromoClosedReason close_reason = FeaturePromoClosedReason::kAbortPromo;

  EXPECT_CALL(custom_action_callback_, Run).Times(0);

  user_education::FeaturePromoParams params(kCustomActionTestFeature);
  params.close_callback =
      base::BindLambdaForTesting([this, &called, &close_reason]() {
        called = true;
        EXPECT_TRUE(promo_controller()->HasPromoBeenDismissed(
            kCustomActionTestFeature, &close_reason));
      });

  RunTestSequence(
      MaybeShowPromo(std::move(params),
                     user_education::FeaturePromoResult::Success()),
      PressDefaultPromoButton(), CheckVariable(called, true),
      CheckVariable(close_reason, FeaturePromoClosedReason::kDismiss));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CallbackHappensAfterCustomAction) {
  bool called = false;
  FeaturePromoClosedReason close_reason = FeaturePromoClosedReason::kAbortPromo;

  user_education::FeaturePromoParams params(kLegalNoticeTestFeature);
  params.close_callback =
      base::BindLambdaForTesting([this, &called, &close_reason]() {
        // Normal promos will defer writing close data until the promo is
        // fully ended.
        called = true;
        EXPECT_TRUE(promo_controller()->HasPromoBeenDismissed(
            kLegalNoticeTestFeature, &close_reason));
      });

  RunTestSequence(
      MaybeShowPromo(std::move(params),
                     user_education::FeaturePromoResult::Success()),
      PressNonDefaultPromoButton(), CheckVariable(called, true),
      CheckVariable(close_reason, FeaturePromoClosedReason::kAction));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       ShowCustomHelpBubble) {
  const auto kBubbleId =
      user_education::test::TestCustomHelpBubbleView::kBubbleId;
  RunTestSequence(
      MaybeShowPromo(kCustomUiTestFeature, CustomHelpBubbleShown{kBubbleId}),
      CheckView(kBubbleId,
                [](views::BubbleDialogDelegateView* bubble) {
                  return bubble->GetBubbleFrameView()->GetDisplayVisibleArrow();
                }),
      WithView(kBubbleId,
               [](views::View* view) { view->GetWidget()->Close(); }),
      WaitForHide(kBubbleId), CheckPromoRequested(kCustomUiTestFeature, false),
      CheckMetrics(kCustomUiTestFeature, ExpectedMetrics()),
      CheckResult(
          [this]() {
            return user_action_tester_.GetActionCount(
                std::string(
                    "UserEducation.MessageAction.AbortedByBubbleDestroyed.")
                    .append(kCustomUiTestFeature.name));
          },
          1));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       ShowCustomHelpBubble_Dismiss) {
  const auto kBubbleId =
      user_education::test::TestCustomHelpBubbleView::kBubbleId;
  RunTestSequence(
      MaybeShowPromo(kCustomUiTestFeature, CustomHelpBubbleShown{kBubbleId}),
      PressButton(
          user_education::test::TestCustomHelpBubbleView::kDismissButtonId),
      WaitForHide(kBubbleId), CheckPromoRequested(kCustomUiTestFeature, false),
      CheckMetrics(kCustomUiTestFeature, ExpectedMetrics{.dismiss_count = 1}));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       ShowCustomHelpBubble_Snooze) {
  const auto kBubbleId =
      user_education::test::TestCustomHelpBubbleView::kBubbleId;
  RunTestSequence(
      MaybeShowPromo(kCustomUiTestFeature, CustomHelpBubbleShown{kBubbleId}),
      PressButton(
          user_education::test::TestCustomHelpBubbleView::kSnoozeButtonId),
      WaitForHide(kBubbleId), CheckPromoRequested(kCustomUiTestFeature, false),
      CheckMetrics(kCustomUiTestFeature, ExpectedMetrics{.snooze_count = 1}));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       ShowCustomHelpBubble_Cancel) {
  const auto kBubbleId =
      user_education::test::TestCustomHelpBubbleView::kBubbleId;
  RunTestSequence(
      MaybeShowPromo(kCustomUiTestFeature, CustomHelpBubbleShown{kBubbleId}),
      PressButton(
          user_education::test::TestCustomHelpBubbleView::kCancelButtonId),
      WaitForHide(kBubbleId), CheckPromoRequested(kCustomUiTestFeature, false),
      CheckMetrics(kCustomUiTestFeature, ExpectedMetrics{.cancel_count = 1}));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       ShowCustomHelpBubble_PressEsc) {
  const auto kBubbleId =
      user_education::test::TestCustomHelpBubbleView::kBubbleId;
  RunTestSequence(
      MaybeShowPromo(kCustomUiTestFeature, CustomHelpBubbleShown{kBubbleId}),
      PressEscAndWaitForClose(kBubbleId),
      CheckMetrics(kCustomUiTestFeature, ExpectedMetrics{.cancel_count = 1}));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       ShowCustomHelpBubble_Action) {
  const auto kBubbleId =
      user_education::test::TestCustomHelpBubbleView::kBubbleId;
  RunTestSequence(
      MaybeShowPromo(kCustomUiTestFeature, CustomHelpBubbleShown{kBubbleId}),
      PressButton(
          user_education::test::TestCustomHelpBubbleView::kActionButtonId),
      WaitForHide(kBubbleId), Check([this]() {
        const bool result = continued_promo_handle_.is_valid();
        continued_promo_handle_.Release();
        return result;
      }),
      CheckPromoRequested(kCustomUiTestFeature, false),
      CheckMetrics(kCustomUiTestFeature,
                   ExpectedMetrics{.custom_action_count = 1}));
}

class BrowserFeaturePromoControllerWithFailuresUiTest
    : public BrowserFeaturePromoControllerUiTest {
 public:
  BrowserFeaturePromoControllerWithFailuresUiTest() {
    feature_promo_test_impl().set_use_shortened_timeouts_for_internal_testing(
        true);
  }
  ~BrowserFeaturePromoControllerWithFailuresUiTest() override = default;

 private:
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerWithFailuresUiTest,
                       EnterprisePolicyBlocksSomePromos) {
  g_browser_process->local_state()->SetBoolean(prefs::kPromotionsEnabled,
                                               false);

  RunTestSequence(
      MaybeShowPromo(kCustomActionTestFeature,
                     user_education::FeaturePromoResult::kBlockedByContext),
      MaybeShowPromo(kToastTestFeature), PressClosePromoButton(),
      MaybeShowPromo(kLegalNoticeTestFeature));
}

MATCHER_P(MatchesContext, expected, "Matches the expected context") {
  return arg.get() == expected.get();
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CustomActionCallbackInSecondWindow) {
  // Create a second browser.
  Browser* const other = CreateBrowser(browser()->profile());

  // Hide the anchor element in the first browser.
  auto* const app_menu_button = BrowserElementsViews::From(browser())->GetView(
      kToolbarAppMenuButtonElementId);
  app_menu_button->SetVisible(false);

  auto& context = BrowserUserEducationInterface::From(other)
                      ->GetUserEducationContextForTesting();
  EXPECT_CALL(custom_action_callback_, Run(MatchesContext(context), testing::_))
      .Times(1);

  RunTestSequence(InAnyContext(
      // This will always try to trigger the promo from the original `browser()`
      // because the default context is always checked first in Kombucha, and
      // the original window is still visible.
      //
      // However, the bubble can only show in the `other` browser because we hid
      // the first browser's app menu button (and the IPH specifies that it need
      // not show in the original context).
      MaybeShowPromo(kCustomActionTestFeature),
      // Perform the action, and verify that the second browser's context is
      // used when the action button is clicked.
      PressNonDefaultPromoButton()));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerUiTest,
                       CustomActionCallbackInSecondWindowAfterFirstCloses) {
  // Create a second browser.
  Browser* const other = CreateBrowser(browser()->profile());

  // Hide the anchor element in the first browser.
  auto* const app_menu_button = BrowserElementsViews::From(browser())->GetView(
      kToolbarAppMenuButtonElementId);
  app_menu_button->SetVisible(false);

  // The promo should now show in the second window.
  auto& context = BrowserUserEducationInterface::From(other)
                      ->GetUserEducationContextForTesting();
  EXPECT_CALL(custom_action_callback_, Run(MatchesContext(context), testing::_))
      .Times(1);

  RunTestSequence(InAnyContext(
      MaybeShowPromo(kCustomActionTestFeature),
      Do([this]() { browser()->window()->Close(); }),
      WaitForHide(kBrowserViewElementId).SetTransitionOnlyOnEvent(true),
      EnsurePresent(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      PressNonDefaultPromoButton()));
}

class BrowserFeaturePromoControllerLiveTrackerUiTest
    : public InteractiveFeaturePromoTest {
 public:
  static const base::Feature& kFeature;

  BrowserFeaturePromoControllerLiveTrackerUiTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos({kFeature}),
                                    ClockMode::kUseDefaultClock) {}

  ~BrowserFeaturePromoControllerLiveTrackerUiTest() override = default;

  BrowserFeaturePromoControllerLiveTrackerUiTest(
      const BrowserFeaturePromoControllerLiveTrackerUiTest&) = delete;
  BrowserFeaturePromoControllerLiveTrackerUiTest& operator=(
      const BrowserFeaturePromoControllerLiveTrackerUiTest&) = delete;

 private:
  base::test::ScopedFeatureList feature_list_;
};

const base::Feature& BrowserFeaturePromoControllerLiveTrackerUiTest::kFeature =
    feature_engagement::kIPHBackNavigationMenuFeature;

// Regression test with live tracker for https://crbug.com/396344371
IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerLiveTrackerUiTest,
                       ShowPromoTwice) {
  RunTestSequence(
      WithView(kBrowserViewElementId,
               [](BrowserView* browser_view) {
                 BrowserUserEducationInterface::From(browser_view->browser())
                     ->MaybeShowFeaturePromo(kFeature);
               }),
      WithView(kBrowserViewElementId,
               [](BrowserView* browser_view) {
                 BrowserUserEducationInterface::From(browser_view->browser())
                     ->MaybeShowFeaturePromo(kFeature);
               }),
      WaitForPromo(kFeature));
}

namespace {

BASE_FEATURE(kIPHExemptFromOmniboxFeature,
             "TEST_ExemptFromOmnibox",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHExemptFromUserNotActiveFeature,
             "TEST_ExemptFromUserNotActive",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHExemptFromToolbarNotCollapsedFeature,
             "TEST_ExemptFromToolbarNotCollapsed",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

class BrowserFeaturePromoControllerWithPromosUiTest
    : public BrowserFeaturePromoControllerUiTestBase {
 public:
  BrowserFeaturePromoControllerWithPromosUiTest() {
    feature_promo_test_impl().set_use_shortened_timeouts_for_internal_testing(
        true);
  }
  ~BrowserFeaturePromoControllerWithPromosUiTest() override = default;

  void SetUpOnMainThread() override {
    BrowserFeaturePromoControllerUiTestBase::SetUpOnMainThread();

    auto spec = FeaturePromoSpecification::CreateForSnoozePromo(
        kIPHExemptFromOmniboxFeature, kToolbarAppMenuButtonElementId,
        IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE);
    spec.AddPreconditionExemption(kOmniboxNotOpenPrecondition);
    RegisterTestFeature(browser(), std::move(spec));

    spec = FeaturePromoSpecification::CreateForSnoozePromo(
        kIPHExemptFromUserNotActiveFeature, kToolbarAppMenuButtonElementId,
        IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE);
    spec.AddPreconditionExemption(kUserNotActivePrecondition);
    RegisterTestFeature(browser(), std::move(spec));

    spec = FeaturePromoSpecification::CreateForSnoozePromo(
        kIPHExemptFromToolbarNotCollapsedFeature,
        kToolbarAppMenuButtonElementId, IDS_TUTORIAL_TAB_GROUP_EDIT_BUBBLE);
    spec.AddPreconditionExemption(kToolbarNotCollapsedPrecondition);
    RegisterTestFeature(browser(), std::move(spec));

    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        views::GetRootWindow(browser_view->GetWidget()),
        browser_view->GetNativeWindow());
  }

 protected:
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerWithPromosUiTest,
                       PromoExemptFromOmniboxNotOpenPrecondition) {
  RunTestSequence(
      WithView(
          kBrowserViewElementId,
          [](BrowserView* browser_view) {
            AutocompleteInput input(
                u"chrome", metrics::OmniboxEventProto::NTP,
                ChromeAutocompleteSchemeClassifier(browser_view->GetProfile()));
            browser_view->GetLocationBarView()
                ->GetOmniboxController()
                ->autocomplete_controller()
                ->Start(input);
          }),
      MaybeShowPromo(kIPHExemptFromUserNotActiveFeature,
                     user_education::FeaturePromoResult::kBlockedByUi),
      MaybeShowPromo(kIPHExemptFromOmniboxFeature));
}

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerWithPromosUiTest,
                       PromoExemptFromUserNotActivePrecondition) {
  RunTestSequence(
      WaitForShow(kBrowserViewElementId), Check([this]() {
        // Use a keypress that is is not an accelerator but won't open the
        // omnibox.
        return ui_test_utils::SendKeyPressSync(browser(),
                                               ui::KeyboardCode::VKEY_SPACE,
                                               false, false, false, false);
      }),
      MaybeShowPromo(
          kIPHExemptFromOmniboxFeature,
          user_education::FeaturePromoResult::kBlockedByUserActivity),
      MaybeShowPromo(kIPHExemptFromUserNotActiveFeature));
}

class BrowserFeaturePromoControllerOverflowUiTest
    : public BrowserFeaturePromoControllerWithPromosUiTest {
 public:
  BrowserFeaturePromoControllerOverflowUiTest() {
    // This has to be called before the browser is created.
    ToolbarControllerUtil::SetPreventOverflowForTesting(false);
  }
  ~BrowserFeaturePromoControllerOverflowUiTest() override = default;
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerOverflowUiTest,
                       PromoExemptFromToolbarNotCollapsed) {
  RunTestSequence(
      WaitForShow(kBrowserViewElementId),
      WithView(
          kBrowserViewElementId,
          [](BrowserView* browser_view) {
            const ToolbarController* const controller =
                browser_view->toolbar()->toolbar_controller();
            CHECK(controller);
            auto* const forward_button =
                views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                    kToolbarForwardButtonElementId,
                    browser_view->GetElementContext());
            auto* const container_view =
                views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
                    ToolbarView::kToolbarElementId,
                    browser_view->GetElementContext());
            constexpr gfx::Size kButtonSize{16, 16};
            while (forward_button->GetVisible()) {
              auto* const button = container_view->AddChildView(
                  std::make_unique<ToolbarButton>());
              button->SetPreferredSize(kButtonSize);
              button->SetMinSize(kButtonSize);
              button->GetViewAccessibility().SetName(u"dummy");
              button->SetVisible(true);
              views::test::RunScheduledLayout(browser_view);
            }
          }),
      WaitForShow(kToolbarOverflowButtonElementId),
      MaybeShowPromo(kIPHExemptFromOmniboxFeature,
                     user_education::FeaturePromoResult::kWindowTooSmall),
      MaybeShowPromo(kIPHExemptFromToolbarNotCollapsedFeature));
}

namespace {

// Class that allows the injection of a startup promo when the browser user
// education interface is initialized.
class BrowserUserEducationInterfaceWithStartupPromo
    : public BrowserUserEducationInterfaceImpl,
      public views::ViewObserver {
 public:
  BrowserUserEducationInterfaceWithStartupPromo(
      BrowserWindowInterface* browser,
      user_education::FeaturePromoParams startup_promo_params)
      : BrowserUserEducationInterfaceImpl(browser),
        startup_promo_params_(std::move(startup_promo_params)) {}
  ~BrowserUserEducationInterfaceWithStartupPromo() override = default;

  void Init(BrowserView* browser_view) override {
    BrowserUserEducationInterfaceImpl::Init(browser_view);
    browser_view_observation_.Observe(browser_view);
  }

 private:
  void OnViewAddedToWidget(views::View* observed_view) override {
    browser_view_observation_.Reset();
    // This is about the same point where startup promos are queued; when the
    // BrowserView is added to the widget, but after browser window features are
    // initialized.
    MaybeShowStartupFeaturePromo(std::move(startup_promo_params_));
  }

  user_education::FeaturePromoParams startup_promo_params_;
  base::ScopedObservation<views::View, views::ViewObserver>
      browser_view_observation_{this};
};

}  // namespace

// Regression test for startup promo issues on User Education 2.5.
// See https://crbug.com/439030167 for more information.
class BrowserFeaturePromoControllerLiveStartupTest
    : public InteractiveFeaturePromoTest {
 public:
  // This will be the feature to use for startup tests below.
  // It should (a) anchor to something that is visible at/near startup, (b) be a
  // toast, and (c) not be dependent on any other flags, features, etc.
  static const base::Feature& GetStartupTestFeature() {
    return feature_engagement::kIPHReadingListDiscoveryFeature;
  }

  BrowserFeaturePromoControllerLiveStartupTest()
      : InteractiveFeaturePromoTest(
            UseDefaultTrackerAllowingPromos({GetStartupTestFeature()}),
            ClockMode::kUseDefaultClock,
            InitialSessionState::kInsideGracePeriod) {}
  ~BrowserFeaturePromoControllerLiveStartupTest() override = default;

  void SetUp() override {
    user_ed_override_ =
        BrowserWindowFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting(base::BindRepeating(
                &BrowserFeaturePromoControllerLiveStartupTest::CreateUserEd,
                base::Unretained(this)));
    InteractiveFeaturePromoTest::SetUp();
  }

  void WaitForPromo() {
    if (!got_result_) {
      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }

 private:
  std::unique_ptr<BrowserUserEducationInterface> CreateUserEd(
      BrowserWindowInterface& browser) {
    user_education::FeaturePromoParams params(GetStartupTestFeature());
    params.show_promo_result_callback = base::BindOnce(
        &BrowserFeaturePromoControllerLiveStartupTest::OnShowPromoResult,
        base::Unretained(this));
    return std::make_unique<BrowserUserEducationInterfaceWithStartupPromo>(
        &browser, std::move(params));
  }

  void OnShowPromoResult(user_education::FeaturePromoResult result) {
    EXPECT_EQ(user_education::FeaturePromoResult::Success(), result);
    got_result_ = true;
    if (run_loop_) {
      run_loop_->Quit();
    }
  }

  bool got_result_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
  ui::UserDataFactory::ScopedOverride user_ed_override_;
};

IN_PROC_BROWSER_TEST_F(BrowserFeaturePromoControllerLiveStartupTest,
                       CheckStartupPromo) {
  WaitForPromo();
}
