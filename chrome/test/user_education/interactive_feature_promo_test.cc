// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/user_education/interactive_feature_promo_test.h"

#include <sstream>
#include <utility>
#include <variant>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/browser/user_education/user_education_service_factory.h"
#include "chrome/test/interaction/feature_engagement_initialized_observer.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test_internal.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_specification.h"
#include "components/user_education/views/help_bubble_view.h"

InteractiveFeaturePromoTestApi::InteractiveFeaturePromoTestApi(
    TrackerMode tracker_mode,
    ClockMode clock_mode,
    InitialSessionState initial_session_state)
    : InteractiveBrowserTestApi(
          std::make_unique<internal::InteractiveFeaturePromoTestPrivate>(
              std::make_unique<InteractionTestUtilBrowser>(),
              std::move(tracker_mode),
              clock_mode,
              initial_session_state)) {}

InteractiveFeaturePromoTestApi::~InteractiveFeaturePromoTestApi() = default;

InteractiveFeaturePromoTestApi::MockTracker*
InteractiveFeaturePromoTestApi::GetMockTrackerFor(Browser* browser) {
  return test_impl().GetMockTrackerFor(browser);
}

void InteractiveFeaturePromoTestApi::RegisterTestFeature(
    Browser* browser,
    user_education::FeaturePromoSpecification spec) {
  UserEducationServiceFactory::GetForBrowserContext(browser->profile())
      ->feature_promo_registry()
      .RegisterFeature(std::move(spec));
}

InteractiveFeaturePromoTestApi::MultiStep
InteractiveFeaturePromoTestApi::WaitForFeatureEngagementReady() {
  // Use a refcounted value to pass information between steps, since there is no
  // access to the browser proper in the pure API class.
  auto browser = base::MakeRefCounted<base::RefCountedData<Browser*>>(nullptr);
  auto steps = Steps(
      // Ensure that the correct tracker for the current context is used.
      WithView(kBrowserViewElementId,
               [this, browser](BrowserView* browser_view) {
                 browser->data = browser_view->browser();
                 CHECK(!test_impl().GetMockTrackerFor(browser->data));
               }),
      ObserveState(kFeatureEngagementInitializedState,
                   [browser]() { return browser->data; }),
      WaitForState(kFeatureEngagementInitializedState, true), FlushEvents(),
      StopObservingState(kFeatureEngagementInitializedState));
  AddDescription(steps, "WaitForFeatureEngagementReady() - %s");
  return steps;
}

InteractiveFeaturePromoTestApi::StepBuilder
InteractiveFeaturePromoTestApi::AdvanceTime(NewTime time) {
  return std::move(Do([this, time]() {
                     test_impl().AdvanceTime(time);
                   }).SetDescription("AdvanceTime()"));
}

InteractiveFeaturePromoTestApi::StepBuilder
InteractiveFeaturePromoTestApi::UpdateIdleState(NewTime time,
                                                bool screen_locked) {
  return std::move(Do([this, time, screen_locked]() {
                     test_impl().UpdateIdleState(time, screen_locked);
                   }).SetDescription("UpdateIdleState()"));
}

InteractiveFeaturePromoTestApi::MultiStep
InteractiveFeaturePromoTestApi::MaybeShowPromo(
    const base::Feature& iph_feature,
    user_education::FeaturePromoResult expected_result,
    base::OnceClosure close_callback) {
  // Always attempt to show the promo.
  auto steps = Steps(std::move(
      CheckView(
          kBrowserViewElementId,
          [this, &iph_feature, expected_result,
           close_callback =
               std::move(close_callback)](BrowserView* browser_view) mutable {
            // If using a mock tracker, ensure that it returns the correct
            // status.
            auto* const tracker =
                test_impl().GetMockTrackerFor(browser_view->browser());
            if (tracker) {
              if (expected_result) {
                EXPECT_CALL(*tracker,
                            ShouldTriggerHelpUI(testing::Ref(iph_feature)))
                    .WillOnce(testing::Return(true));
              } else if (user_education::FeaturePromoResult::kBlockedByConfig ==
                         expected_result) {
                EXPECT_CALL(*tracker,
                            ShouldTriggerHelpUI(testing::Ref(iph_feature)))
                    .WillOnce(testing::Return(false));
              } else {
                EXPECT_CALL(*tracker,
                            ShouldTriggerHelpUI(testing::Ref(iph_feature)))
                    .Times(0);
              }
            }

            // Attempt to show the promo.
            user_education::FeaturePromoParams params(iph_feature);
            params.close_callback = std::move(close_callback);
            const auto result =
                browser_view->MaybeShowFeaturePromo(std::move(params));

            // If the promo showed, expect it to be dismissed at some point.
            if (result && tracker) {
              EXPECT_CALL(*tracker, Dismissed(testing::Ref(iph_feature)));
            }
            return result;
          },
          expected_result)
          .SetDescription("Try to show promo")));

  // If success is expected, add steps to wait for the bubble to be shown and
  // verify that the correct promo is showing.
  if (expected_result) {
    steps = Steps(std::move(steps), WaitForPromo(iph_feature));
  }

  std::ostringstream desc;
  desc << "MaybeShowPromo(" << iph_feature.name << ", " << expected_result
       << ") - %s";
  AddDescription(steps, desc.str());
  return steps;
}

InteractiveFeaturePromoTestApi::MultiStep
InteractiveFeaturePromoTestApi::WaitForPromo(const base::Feature& iph_feature) {
  auto steps = Steps(
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      FlushEvents(),
      CheckView(
          kBrowserViewElementId, [&iph_feature](BrowserView* browser_view) {
            return browser_view->GetFeaturePromoController()->IsPromoActive(
                iph_feature);
          }));

  std::ostringstream desc;
  desc << "WaitForPromo(" << iph_feature.name << ") - %s";
  AddDescription(steps, desc.str());
  return steps;
}

InteractiveFeaturePromoTestApi::MultiStep
InteractiveFeaturePromoTestApi::AbortPromo(const base::Feature& iph_feature,
                                           bool expected_result) {
  auto steps = Steps(CheckView(
      kBrowserViewElementId,
      [&iph_feature](BrowserView* browser_view) {
        return browser_view->CloseFeaturePromo(
            iph_feature, user_education::EndFeaturePromoReason::kAbortPromo);
      },
      expected_result));
  if (expected_result) {
    steps = Steps(
        std::move(steps),
        WaitForHide(
            user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
        FlushEvents());
  }

  std::ostringstream desc;
  desc << "AbortPromo(" << iph_feature.name << ", " << expected_result
       << ") - %s";
  AddDescription(steps, desc.str());
  return steps;
}

InteractiveFeaturePromoTestApi::MultiStep
InteractiveFeaturePromoTestApi::PressClosePromoButton() {
  auto steps = Steps(
      PressButton(user_education::HelpBubbleView::kCloseButtonIdForTesting),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      FlushEvents());
  AddDescription(steps, "PressClosePromoButton() - %s");
  return steps;
}

InteractiveFeaturePromoTestApi::MultiStep
InteractiveFeaturePromoTestApi::PressDefaultPromoButton() {
  auto steps = Steps(
      PressButton(user_education::HelpBubbleView::kDefaultButtonIdForTesting),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      FlushEvents());
  AddDescription(steps, "PressDefaultPromoButton() - %s");
  return steps;
}

InteractiveFeaturePromoTestApi::MultiStep
InteractiveFeaturePromoTestApi::PressNonDefaultPromoButton() {
  auto steps = Steps(
      PressButton(
          user_education::HelpBubbleView::kFirstNonDefaultButtonIdForTesting),
      WaitForHide(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      FlushEvents());
  AddDescription(steps, "PressNonDefaultPromoButton() - %s");
  return steps;
}
