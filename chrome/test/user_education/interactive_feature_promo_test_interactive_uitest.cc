// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace {
BASE_FEATURE(kTestIphFeature,
             "TestIphFeature",
             base::FEATURE_ENABLED_BY_DEFAULT);
}

class InteractiveFeaturePromoTestUiTest : public InteractiveFeaturePromoTest {
 public:
  InteractiveFeaturePromoTestUiTest() = default;
  ~InteractiveFeaturePromoTestUiTest() override = default;

  auto ShowPromo() {
    return WithView(kBrowserViewElementId, [this](BrowserView* browser) {
      EXPECT_CALL(*GetMockTrackerFor(browser->browser()),
                  ShouldTriggerHelpUI(testing::Ref(kTestIphFeature)))
          .WillOnce(testing::Return(true));
      browser->MaybeShowFeaturePromo(kTestIphFeature);
    });
  }
};

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest, CheckPromoIsActive) {
  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  RegisterTestFeature(browser(), std::move(spec));

  RunTestSequence(
      ShowPromo(),
      WaitForShow(
          user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
      CheckPromoIsActive(kTestIphFeature));
}

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest,
                       CheckPromoIsActiveFails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  RegisterTestFeature(browser(), std::move(spec));

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  EXPECT_CALL_IN_SCOPE(aborted, Run,
                       RunTestSequence(CheckPromoIsActive(kTestIphFeature)));
}

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest,
                       CheckPromoIsActiveInDifferentContext) {
  auto* const other = CreateBrowser(browser()->profile());

  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  RegisterTestFeature(other, std::move(spec));

  RunTestSequence(InContext(
      other->window()->GetElementContext(),
      Steps(ShowPromo(),
            WaitForShow(
                user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
            CheckPromoIsActive(kTestIphFeature))));
}

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest,
                       CheckPromoIsActiveInDifferentContextFails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  auto* const other = CreateBrowser(browser()->profile());

  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  RegisterTestFeature(browser(), std::move(spec));

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(
          ShowPromo(),
          WaitForShow(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
          InContext(other->window()->GetElementContext(),
                    CheckPromoIsActive(kTestIphFeature))));
}

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest,
                       CheckPromoIsActiveInDifferentContextFromController) {
  auto* const other = CreateBrowser(browser()->profile());

  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  spec.SetInAnyContext(true);
  spec.SetAnchorElementFilter(base::BindLambdaForTesting(
      [other](const ui::ElementTracker::ElementList& elements)
          -> ui::TrackedElement* {
        for (auto* element : elements) {
          if (element->context() == other->window()->GetElementContext()) {
            return element;
          }
        }
        return nullptr;
      }));
  RegisterTestFeature(browser(), std::move(spec));

  RunTestSequence(
      ShowPromo(),
      InAnyContext(Steps(
          WaitForShow(
              user_education::HelpBubbleView::kHelpBubbleElementIdForTesting),
          CheckPromoIsActive(kTestIphFeature))));
}

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest,
                       CheckPromoIsActiveInAnyContextFails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  auto* const other = CreateBrowser(browser()->profile());

  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  spec.SetInAnyContext(true);
  spec.SetAnchorElementFilter(base::BindLambdaForTesting(
      [other](const ui::ElementTracker::ElementList& elements)
          -> ui::TrackedElement* {
        for (auto* element : elements) {
          if (element->context() == other->window()->GetElementContext()) {
            return element;
          }
        }
        return nullptr;
      }));
  RegisterTestFeature(browser(), std::move(spec));

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(InAnyContext(CheckPromoIsActive(kTestIphFeature))));
}

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest, WaitForPromo) {
  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  RegisterTestFeature(browser(), std::move(spec));

  RunTestSequence(ShowPromo(), WaitForPromo(kTestIphFeature));
}

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest,
                       WaitForPromoInDifferentContext) {
  auto* const other = CreateBrowser(browser()->profile());

  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  RegisterTestFeature(other, std::move(spec));

  RunTestSequence(InContext(other->window()->GetElementContext(),
                            Steps(ShowPromo(), WaitForPromo(kTestIphFeature))));
}

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest,
                       WaitForPromoInDifferentContextFromController) {
  auto* const other = CreateBrowser(browser()->profile());

  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  spec.SetInAnyContext(true);
  spec.SetAnchorElementFilter(base::BindLambdaForTesting(
      [other](const ui::ElementTracker::ElementList& elements)
          -> ui::TrackedElement* {
        for (auto* element : elements) {
          if (element->context() == other->window()->GetElementContext()) {
            return element;
          }
        }
        return nullptr;
      }));
  RegisterTestFeature(browser(), std::move(spec));

  RunTestSequence(ShowPromo(), InAnyContext(WaitForPromo(kTestIphFeature)));
}
