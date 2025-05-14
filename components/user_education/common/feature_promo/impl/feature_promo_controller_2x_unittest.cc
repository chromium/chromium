// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_20.h"
#include "components/user_education/common/feature_promo/impl/feature_promo_controller_25.h"
#include "components/user_education/test/feature_promo_controller_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"

namespace user_education {

namespace {

BASE_FEATURE(kIPHTestLowPriorityToast,
             "IPH_TestLowPriorityToast",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHTestLowPrioritySnooze,
             "IPH_TestLowPrioritySnooze",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHTestActionable,
             "IPH_TestActionable",
             base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kIPHTestLegalNotice,
             "IPH_TestLegalNotice",
             base::FEATURE_ENABLED_BY_DEFAULT);

using PriorityInfo = FeaturePromoPriorityProvider::PromoPriorityInfo;
using PromoPriority = FeaturePromoPriorityProvider::PromoPriority;
using PromoWeight = FeaturePromoPriorityProvider::PromoWeight;

// Implementation of V2.5 controller that cleans itself up properly.
// Normally this would be done by a derived class.
class TestFeaturePromoController25 : public FeaturePromoController25 {
 public:
  using FeaturePromoController25::FeaturePromoController25;
  ~TestFeaturePromoController25() override { OnDestroying(); }
};

}  // namespace

enum class PromoControllerVersion { kV20, kV25 };

// Tests that ensure that queueing promos as implemented by
// `MaybeShowStartupPromo()` behave as expected in FeaturePromoController for
// V2.0 and V2.5, noting where there are specific differences in behavior.
class FeaturePromoControllerQueueTest
    : public test::FeaturePromoControllerTestBase,
      public testing::WithParamInterface<PromoControllerVersion> {
 public:
  FeaturePromoControllerQueueTest() = default;
  ~FeaturePromoControllerQueueTest() override = default;

  void SetUp() override {
    FeaturePromoControllerTestBase::SetUp();
    promo_registry().RegisterFeature(
        FeaturePromoSpecification::CreateForTesting(kIPHTestLowPriorityToast,
                                                    kAnchorElementId, IDS_OK));
    promo_registry().RegisterFeature(
        FeaturePromoSpecification::CreateForTesting(
            kIPHTestLowPrioritySnooze, kAnchorElementId, IDS_OK,
            FeaturePromoSpecification::PromoType::kSnooze));
    promo_registry().RegisterFeature(
        FeaturePromoSpecification::CreateForTesting(
            kIPHTestActionable, kAnchorElementId, IDS_OK,
            FeaturePromoSpecification::PromoType::kCustomAction,
            FeaturePromoSpecification::PromoSubtype::kActionableAlert,
            custom_action_callback_.Get()));
    promo_registry().RegisterFeature(
        FeaturePromoSpecification::CreateForTesting(
            kIPHTestLegalNotice, kAnchorElementId, IDS_OK,
            FeaturePromoSpecification::PromoType::kToast,
            FeaturePromoSpecification::PromoSubtype::kLegalNotice));
  }

 protected:
  // FeaturePromoControllerTestBase:
  std::unique_ptr<FeaturePromoControllerCommon> CreateController() override {
    switch (GetParam()) {
      case PromoControllerVersion::kV20: {
        return std::make_unique<TestPromoController<FeaturePromoController20>>(
            &tracker(), &promo_registry(), &help_bubble_factory_registry(),
            &storage_service(), &session_policy(), &tutorial_service(),
            &messaging_controller());
      }
      case PromoControllerVersion::kV25: {
        auto result =
            std::make_unique<TestPromoController<TestFeaturePromoController25>>(
                &tracker(), &promo_registry(), &help_bubble_factory_registry(),
                &storage_service(), &session_policy(), &tutorial_service(),
                &messaging_controller());
        result->Init();
        return result;
      }
    }
  }

  base::MockCallback<FeaturePromoSpecification::CustomActionCallback>
      custom_action_callback_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    FeaturePromoControllerQueueTest,
    testing::Values(PromoControllerVersion::kV20, PromoControllerVersion::kV25),
    [](const testing::TestParamInfo<PromoControllerVersion>& param) {
      switch (param.param) {
        case PromoControllerVersion::kV20:
          return "V2_point_0";
        case PromoControllerVersion::kV25:
          return "V2_point_5";
      }
    });

TEST_P(FeaturePromoControllerQueueTest, QueuePromo) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  FeaturePromoParams params(kIPHTestLowPriorityToast);
  params.show_promo_result_callback = result.Get();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      result, Run(FeaturePromoResult::Success()),
      promo_controller().MaybeShowStartupPromo(std::move(params)));
  EXPECT_NE(GetHelpBubble(), nullptr);
}

// Regression test for https://crbug.com/417487540.
TEST_P(FeaturePromoControllerQueueTest, QueuePromoTwice) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);

  FeaturePromoParams params(kIPHTestLowPrioritySnooze);
  params.show_promo_result_callback = result.Get();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      result, Run(FeaturePromoResult::Success()),
      promo_controller().MaybeShowStartupPromo(std::move(params)));
  EXPECT_TRUE(promo_controller().IsPromoActive(kIPHTestLowPrioritySnooze));

  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      result2, Run(FeaturePromoResult(FeaturePromoResult::kAlreadyQueued)),
      promo_controller().MaybeShowStartupPromo(std::move(params2)));
  EXPECT_TRUE(promo_controller().IsPromoActive(kIPHTestLowPrioritySnooze));
}

TEST_P(FeaturePromoControllerQueueTest, QueueTwoPromosTogetherBothAreEligible) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  FeaturePromoParams params(kIPHTestLowPriorityToast);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();
  EXPECT_ASYNC_CALL_IN_SCOPE(result, Run(FeaturePromoResult::Success()), {
    promo_controller().MaybeShowStartupPromo(std::move(params));
    promo_controller().MaybeShowStartupPromo(std::move(params2));
  });

  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()),
                             GetHelpBubble()->Close());
}

TEST_P(FeaturePromoControllerQueueTest,
       QueueTwoPromosTogetherAnchorHiddenBeforeFirst) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  FeaturePromoParams params(kIPHTestLowPriorityToast);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();

  anchor_element().Hide();

  switch (GetParam()) {
    case PromoControllerVersion::kV25: {
      // In 2.5, promos will be held until the anchor is visible.
      promo_controller().MaybeShowStartupPromo(std::move(params));
      promo_controller().MaybeShowStartupPromo(std::move(params2));

      // The first promo will not show until the anchor element is present.
      EXPECT_ASYNC_CALL_IN_SCOPE(result, Run(FeaturePromoResult::Success()),
                                 { anchor_element().Show(); });

      // The second promo can show right away.
      EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run, GetHelpBubble()->Close());
      break;
    }
    case PromoControllerVersion::kV20: {
      // In 2.0, promos will fail immediately.
      EXPECT_ASYNC_CALLS_IN_SCOPE_2(
          result,
          Run(FeaturePromoResult(FeaturePromoResult::kAnchorNotVisible)),
          result2,
          Run(FeaturePromoResult(FeaturePromoResult::kAnchorNotVisible)), {
            promo_controller().MaybeShowStartupPromo(std::move(params));
            promo_controller().MaybeShowStartupPromo(std::move(params2));
          });
      break;
    }
  }
}

TEST_P(FeaturePromoControllerQueueTest,
       QueueTwoPromosTogetherAnchorHiddenBeforeSecond) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::BubbleCloseCallback, closed);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  FeaturePromoParams params(kIPHTestLowPriorityToast);
  params.show_promo_result_callback = result.Get();
  params.close_callback = closed.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();
  EXPECT_ASYNC_CALL_IN_SCOPE(result, Run(FeaturePromoResult::Success()), {
    promo_controller().MaybeShowStartupPromo(std::move(params));
    promo_controller().MaybeShowStartupPromo(std::move(params2));
  });

  // Hiding the anchor kills the first promo, and the second cannot start.
  switch (GetParam()) {
    case PromoControllerVersion::kV25: {
      EXPECT_ASYNC_CALL_IN_SCOPE(closed, Run, anchor_element().Hide());

      // Showing the anchor again allows the second promo to show, since it is a
      // "wait-for" condition and not a "required" condition.
      EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run, anchor_element().Show());
      break;
    }
    case PromoControllerVersion::kV20: {
      // In 2.0, second fails immediately.
      EXPECT_ASYNC_CALLS_IN_SCOPE_2(
          closed, Run, result2,
          Run(FeaturePromoResult(FeaturePromoResult::kAnchorNotVisible)),
          anchor_element().Hide());
    }
  }
}

TEST_P(FeaturePromoControllerQueueTest,
       QueueTwoPromosTogetherFirstBlockedByPolicy) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  FeaturePromoParams params(kIPHTestLowPriorityToast);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();

  // This is a fatal error, as this is checked as part of a required condition.
  const FeaturePromoResult kFailure = FeaturePromoResult::kBlockedByCooldown;
  EXPECT_CALL(session_policy(), CanShowPromo(PriorityInfo{PromoWeight::kLight,
                                                          PromoPriority::kLow},
                                             std::optional<PriorityInfo>()))
      .WillRepeatedly(testing::Return(kFailure));

  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      result, Run(kFailure), result2, Run(FeaturePromoResult::Success()), {
        promo_controller().MaybeShowStartupPromo(std::move(params));
        promo_controller().MaybeShowStartupPromo(std::move(params2));
      });
}

TEST_P(FeaturePromoControllerQueueTest, QueueMidThenLowPriority) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  FeaturePromoParams params(kIPHTestActionable);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();

  // Standard behavior is to have one promo wait for the other.
  EXPECT_ASYNC_CALL_IN_SCOPE(result, Run(FeaturePromoResult::Success()), {
    promo_controller().MaybeShowStartupPromo(std::move(params));
    promo_controller().MaybeShowStartupPromo(std::move(params2));
  });
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()),
                             GetHelpBubble()->Close());
}

TEST_P(FeaturePromoControllerQueueTest, QueueLowThenMidPriority) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  FeaturePromoParams params(kIPHTestActionable);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();

  // Standard behavior is to have one promo wait for the other.
  EXPECT_ASYNC_CALL_IN_SCOPE(result, Run(FeaturePromoResult::Success()), {
    // Queue in reverse order from the previous test.
    // The outcomes should still be the same.
    promo_controller().MaybeShowStartupPromo(std::move(params2));
    promo_controller().MaybeShowStartupPromo(std::move(params));
  });
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()),
                             GetHelpBubble()->Close());
}

TEST_P(FeaturePromoControllerQueueTest, QueueHighThenLowPriority) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  FeaturePromoParams params(kIPHTestLegalNotice);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();

  // Standard behavior is to have one promo wait for the other.
  EXPECT_ASYNC_CALL_IN_SCOPE(result, Run(FeaturePromoResult::Success()), {
    promo_controller().MaybeShowStartupPromo(std::move(params));
    promo_controller().MaybeShowStartupPromo(std::move(params2));
  });
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()),
                             GetHelpBubble()->Close());
}

TEST_P(FeaturePromoControllerQueueTest, QueueLowThenHighPriority) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  FeaturePromoParams params(kIPHTestLegalNotice);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();

  // Standard behavior is to have one promo wait for the other.
  EXPECT_ASYNC_CALL_IN_SCOPE(result, Run(FeaturePromoResult::Success()), {
    // Queue in reverse order from the previous test.
    // The outcomes should still be the same.
    promo_controller().MaybeShowStartupPromo(std::move(params2));
    promo_controller().MaybeShowStartupPromo(std::move(params));
  });
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()),
                             GetHelpBubble()->Close());
}

TEST_P(FeaturePromoControllerQueueTest, DemoOverridesOtherPromos) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         demo_result);
  FeaturePromoParams params(kIPHTestLegalNotice);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();
  FeaturePromoParams demo_params(kIPHTestLowPriorityToast);
  demo_params.show_promo_result_callback = demo_result.Get();

  EXPECT_ASYNC_CALLS_IN_SCOPE_3(
      result, Run(FeaturePromoResult(FeaturePromoResult::kBlockedByPromo)),
      result2, Run(FeaturePromoResult(FeaturePromoResult::kBlockedByPromo)),
      demo_result, Run(FeaturePromoResult(FeaturePromoResult::Success())), {
        // Queue in reverse order from the previous test.
        // The outcomes should still be the same.
        promo_controller().MaybeShowStartupPromo(std::move(params));
        promo_controller().MaybeShowStartupPromo(std::move(params2));
        promo_controller().MaybeShowPromoForDemoPage(std::move(demo_params));
      });
}

TEST_P(FeaturePromoControllerQueueTest, ShowHighThenQueueLowPriority) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  FeaturePromoParams params(kIPHTestLegalNotice);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();

  // Standard behavior is to have one promo wait for the other.
  EXPECT_ASYNC_CALL_IN_SCOPE(result, Run(FeaturePromoResult::Success()), {
    promo_controller().MaybeShowStartupPromo(std::move(params));
  });

  promo_controller().MaybeShowStartupPromo(std::move(params2));
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()),
                             GetHelpBubble()->Close());
}

TEST_P(FeaturePromoControllerQueueTest, ShowLowThenQueueHighPriority) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::BubbleCloseCallback, closed);
  FeaturePromoParams params(kIPHTestLegalNotice);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();
  params2.close_callback = closed.Get();

  // Run the low priority (promo 2) first:
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()), {
    promo_controller().MaybeShowStartupPromo(std::move(params2));
  });

  // Queueing the high priority promo should end the other and run this one.
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      closed, Run, result, Run(FeaturePromoResult::Success()),
      { promo_controller().MaybeShowStartupPromo(std::move(params)); });
}

TEST_P(FeaturePromoControllerQueueTest, DemoCancelsExistingPromo) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         demo_result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::BubbleCloseCallback, closed);
  FeaturePromoParams params(kIPHTestLegalNotice);
  params.show_promo_result_callback = result.Get();
  params.close_callback = closed.Get();
  FeaturePromoParams demo_params(kIPHTestLowPrioritySnooze);
  demo_params.show_promo_result_callback = demo_result.Get();

  // Show the first promo.
  EXPECT_ASYNC_CALL_IN_SCOPE(result, Run(FeaturePromoResult::Success()), {
    promo_controller().MaybeShowStartupPromo(std::move(params));
  });

  // Queueing the demo promo should cancel the other promo.
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      closed, Run, demo_result, Run(FeaturePromoResult::Success()), {
        promo_controller().MaybeShowPromoForDemoPage(std::move(demo_params));
      });
}

TEST_P(FeaturePromoControllerQueueTest, DisabledFeature) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(kIPHTestLowPriorityToast);

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  FeaturePromoParams params(kIPHTestLowPriorityToast);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();

  // Disabled feature cannot show.
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      result, Run(FeaturePromoResult(FeaturePromoResult::kFeatureDisabled)),
      result2, Run(FeaturePromoResult::Success()), {
        promo_controller().MaybeShowStartupPromo(std::move(params));
        promo_controller().MaybeShowStartupPromo(std::move(params2));
      });
}

TEST_P(FeaturePromoControllerQueueTest, DisabledFeatureInDemoMode) {
  base::test::ScopedFeatureList list;
  list.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {{feature_engagement::kIPHDemoMode,
        {{feature_engagement::kIPHDemoModeFeatureChoiceParam,
          kIPHTestLowPriorityToast.name}}}},
      /*disabled_features=*/
      {kIPHTestLowPriorityToast});

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  FeaturePromoParams params(kIPHTestLowPriorityToast);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();

  // Disabled feature cannot show EVEN IN DEMO MODE.
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      result, Run(FeaturePromoResult(FeaturePromoResult::kFeatureDisabled)),
      result2, Run(FeaturePromoResult::Success()), {
        promo_controller().MaybeShowStartupPromo(std::move(params));
        promo_controller().MaybeShowStartupPromo(std::move(params2));
      });
}

TEST_P(FeaturePromoControllerQueueTest, DisabledFeatureShownFromDemoPage) {
  base::test::ScopedFeatureList list;
  list.InitAndDisableFeature(kIPHTestLowPriorityToast);

  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  FeaturePromoParams params(kIPHTestLowPriorityToast);
  params.show_promo_result_callback = result.Get();

  // Disabled feature CAN be shown from demo page.
  EXPECT_ASYNC_CALL_IN_SCOPE(
      result, Run(FeaturePromoResult::Success()),
      promo_controller().MaybeShowPromoForDemoPage(std::move(params)));
}

#if !BUILDFLAG(IS_ANDROID)

TEST_P(FeaturePromoControllerQueueTest, FeatureEngagementConfig) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  FeaturePromoParams params(kIPHTestLowPriorityToast);
  params.show_promo_result_callback = result.Get();

  // Create a config state that will block the promo.
  feature_engagement::Tracker::EventList events;
  events.emplace_back(
      feature_engagement::EventConfig{
          "foo",
          feature_engagement::Comparator{
              feature_engagement::ComparatorType::EQUAL, 0},
          365, 365},
      1);
  EXPECT_CALL(tracker(), ListEvents(testing::Ref(kIPHTestLowPriorityToast)))
      .WillOnce(testing::Return(events));

  EXPECT_ASYNC_CALL_IN_SCOPE(
      result, Run(FeaturePromoResult(FeaturePromoResult::kBlockedByConfig)),
      promo_controller().MaybeShowStartupPromo(std::move(params)));
}

#endif

}  // namespace user_education
