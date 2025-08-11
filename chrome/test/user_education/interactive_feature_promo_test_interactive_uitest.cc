// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
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
BASE_FEATURE_PARAM(std::string,
                   kTestIphFeatureParam1,
                   &kTestIphFeature,
                   "x_foo",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kTestIphFeatureParam2,
                   &kTestIphFeature,
                   "x_bar",
                   "");

}  // namespace

class InteractiveFeaturePromoTestUiTest : public InteractiveFeaturePromoTest {
 public:
  InteractiveFeaturePromoTestUiTest() = default;
  ~InteractiveFeaturePromoTestUiTest() override = default;

  auto ShowPromo() {
    return WithView(kBrowserViewElementId, [this](BrowserView* browser) {
      EXPECT_CALL(*GetMockTrackerFor(browser->browser()),
                  ShouldTriggerHelpUI(testing::Ref(kTestIphFeature)))
          .WillRepeatedly(testing::Return(true));
      BrowserUserEducationInterface::From(browser->browser())
          ->MaybeShowFeaturePromo(kTestIphFeature);
    });
  }
};

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest, CheckPromoRequested) {
  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  RegisterTestFeature(browser(), std::move(spec));

  RunTestSequence(ShowPromo(), CheckPromoRequested(kTestIphFeature));
}

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest,
                       CheckPromoRequestedFails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  RegisterTestFeature(browser(), std::move(spec));

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());
  EXPECT_CALL_IN_SCOPE(aborted, Run,
                       RunTestSequence(CheckPromoRequested(kTestIphFeature)));
}

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest,
                       CheckPromoRequestedInDifferentContext) {
  auto* const other = CreateBrowser(browser()->profile());

  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  RegisterTestFeature(other, std::move(spec));

  RunTestSequence(InContext(BrowserElements::From(other)->GetContext(),
                            ShowPromo(), CheckPromoRequested(kTestIphFeature)));
}

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest,
                       CheckPromoRequestedInDifferentContextFromController) {
  auto* const other = CreateBrowser(browser()->profile());

  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  spec.SetInAnyContext(true);
  spec.SetAnchorElementFilter(base::BindLambdaForTesting(
      [other](const ui::ElementTracker::ElementList& elements)
          -> ui::TrackedElement* {
        for (auto* element : elements) {
          if (element->context() ==
              BrowserElements::From(other)->GetContext()) {
            return element;
          }
        }
        return nullptr;
      }));
  RegisterTestFeature(browser(), std::move(spec));

  RunTestSequence(ShowPromo(),
                  InAnyContext(CheckPromoRequested(kTestIphFeature)));
}

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestUiTest,
                       CheckPromoRequestedInAnyContextFails) {
  UNCALLED_MOCK_CALLBACK(ui::InteractionSequence::AbortedCallback, aborted);
  auto* const other = CreateBrowser(browser()->profile());

  auto spec = user_education::FeaturePromoSpecification::CreateForTesting(
      kTestIphFeature, kTopContainerElementId, IDS_SETTINGS);
  spec.SetInAnyContext(true);
  spec.SetAnchorElementFilter(base::BindLambdaForTesting(
      [other](const ui::ElementTracker::ElementList& elements)
          -> ui::TrackedElement* {
        for (auto* element : elements) {
          if (element->context() ==
              BrowserElements::From(other)->GetContext()) {
            return element;
          }
        }
        return nullptr;
      }));
  RegisterTestFeature(browser(), std::move(spec));

  private_test_impl().set_aborted_callback_for_testing(aborted.Get());

  EXPECT_CALL_IN_SCOPE(
      aborted, Run,
      RunTestSequence(InAnyContext(CheckPromoRequested(kTestIphFeature))));
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

  RunTestSequence(InContext(BrowserElements::From(other)->GetContext(),
                            ShowPromo(), WaitForPromo(kTestIphFeature)));
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
          if (element->context() ==
              BrowserElements::From(other)->GetContext()) {
            return element;
          }
        }
        return nullptr;
      }));
  RegisterTestFeature(browser(), std::move(spec));

  RunTestSequence(ShowPromo(), InAnyContext(WaitForPromo(kTestIphFeature)));
}

class InteractiveFeaturePromoTestParamUiTest
    : public InteractiveFeaturePromoTest {
 public:
  InteractiveFeaturePromoTestParamUiTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromosWithParams(
            {{kTestIphFeature, {{"x_foo", "foo"}, {"x_bar", "bar"}}}})) {}
  ~InteractiveFeaturePromoTestParamUiTest() override = default;
};

IN_PROC_BROWSER_TEST_F(InteractiveFeaturePromoTestParamUiTest, SetsParams) {
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTestIphFeature));
  EXPECT_EQ("foo", kTestIphFeatureParam1.Get());
  EXPECT_EQ("bar", kTestIphFeatureParam2.Get());
}
