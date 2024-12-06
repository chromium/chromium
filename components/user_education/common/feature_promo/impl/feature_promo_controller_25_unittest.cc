// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/feature_promo_controller_25.h"

#include <memory>

#include "base/feature_list.h"
#include "base/test/mock_callback.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/test/feature_promo_controller_test_base.h"
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

}  // namespace

class FeaturePromoController25Test
    : public test::FeaturePromoControllerTestBase {
 public:
  FeaturePromoController25Test() = default;
  ~FeaturePromoController25Test() override = default;

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
    auto result =
        std::make_unique<TestPromoController<FeaturePromoController25>>(
            &tracker(), &promo_registry(), &help_bubble_factory_registry(),
            &storage_service(), &session_policy(), &tutorial_service(),
            &messaging_controller());
    result->Init();
    return result;
  }

  base::MockCallback<FeaturePromoSpecification::CustomActionCallback>
      custom_action_callback_;
};

TEST_F(FeaturePromoController25Test, QueuePromo) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  FeaturePromoParams params(kIPHTestLowPriorityToast);
  params.show_promo_result_callback = result.Get();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      result, Run(FeaturePromoResult::Success()),
      promo_controller().MaybeShowPromo(std::move(params)));
  EXPECT_NE(GetHelpBubble(), nullptr);
}

TEST_F(FeaturePromoController25Test, QueueTwoPromosTogetherBothAreEligible) {
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result);
  UNCALLED_MOCK_CALLBACK(FeaturePromoController::ShowPromoResultCallback,
                         result2);
  FeaturePromoParams params(kIPHTestLowPriorityToast);
  params.show_promo_result_callback = result.Get();
  FeaturePromoParams params2(kIPHTestLowPrioritySnooze);
  params2.show_promo_result_callback = result2.Get();
  EXPECT_ASYNC_CALL_IN_SCOPE(result, Run(FeaturePromoResult::Success()), {
    promo_controller().MaybeShowPromo(std::move(params));
    promo_controller().MaybeShowPromo(std::move(params2));
  });
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()),
                             GetHelpBubble()->Close());
}

TEST_F(FeaturePromoController25Test,
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

  promo_controller().MaybeShowPromo(std::move(params));
  promo_controller().MaybeShowPromo(std::move(params2));

  // The first promo will not show until the anchor element is present.
  EXPECT_ASYNC_CALL_IN_SCOPE(result, Run(FeaturePromoResult::Success()),
                             { anchor_element().Show(); });

  // The second promo can show right away.
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run, GetHelpBubble()->Close());
}

TEST_F(FeaturePromoController25Test,
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
    promo_controller().MaybeShowPromo(std::move(params));
    promo_controller().MaybeShowPromo(std::move(params2));
  });

  // Hiding the anchor kills the first promo, and the second cannot start.
  EXPECT_ASYNC_CALL_IN_SCOPE(closed, Run, anchor_element().Hide());

  // Showing the anchor again allows the second promo to show, since it is a
  // "wait-for" condition and not a "required" condition.
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run, anchor_element().Show());
}

TEST_F(FeaturePromoController25Test,
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
        promo_controller().MaybeShowPromo(std::move(params));
        promo_controller().MaybeShowPromo(std::move(params2));
      });
}

TEST_F(FeaturePromoController25Test, QueueMidThenLowPriority) {
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
    promo_controller().MaybeShowPromo(std::move(params));
    promo_controller().MaybeShowPromo(std::move(params2));
  });
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()),
                             GetHelpBubble()->Close());
}

TEST_F(FeaturePromoController25Test, QueueLowThenMidPriority) {
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
    promo_controller().MaybeShowPromo(std::move(params2));
    promo_controller().MaybeShowPromo(std::move(params));
  });
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()),
                             GetHelpBubble()->Close());
}

TEST_F(FeaturePromoController25Test, QueueHighThenLowPriority) {
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
    promo_controller().MaybeShowPromo(std::move(params));
    promo_controller().MaybeShowPromo(std::move(params2));
  });
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()),
                             GetHelpBubble()->Close());
}

TEST_F(FeaturePromoController25Test, QueueLowThenHighPriority) {
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
    promo_controller().MaybeShowPromo(std::move(params2));
    promo_controller().MaybeShowPromo(std::move(params));
  });
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()),
                             GetHelpBubble()->Close());
}

TEST_F(FeaturePromoController25Test, DemoOverridesOtherPromos) {
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
        promo_controller().MaybeShowPromo(std::move(params));
        promo_controller().MaybeShowPromo(std::move(params2));
        promo_controller().MaybeShowPromoForDemoPage(std::move(demo_params));
      });
}

TEST_F(FeaturePromoController25Test, ShowHighThenQueueLowPriority) {
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
    promo_controller().MaybeShowPromo(std::move(params));
  });

  promo_controller().MaybeShowPromo(std::move(params2));
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult::Success()),
                             GetHelpBubble()->Close());
}

TEST_F(FeaturePromoController25Test, ShowLowThenQueueHighPriority) {
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
    promo_controller().MaybeShowPromo(std::move(params2));
  });

  // Queueing the high priority promo should end the other and run this one.
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      closed, Run, result, Run(FeaturePromoResult::Success()),
      { promo_controller().MaybeShowPromo(std::move(params)); });
}

TEST_F(FeaturePromoController25Test, DemoCancelsExistingPromo) {
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
    promo_controller().MaybeShowPromo(std::move(params));
  });

  // Queueing the demo promo should cancel the other promo.
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      closed, Run, demo_result, Run(FeaturePromoResult::Success()), {
        promo_controller().MaybeShowPromoForDemoPage(std::move(demo_params));
      });
}

}  // namespace user_education
