// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/common_preconditions.h"

#include "base/feature_list.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/test/mock_anchor_element_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"

namespace user_education {

TEST(CommonPreconditionsTest,
     FeatureEngagementTrackerInitializedPreconditionFailsNoTracker) {
  FeatureEngagementTrackerInitializedPrecondition precond(nullptr);
  EXPECT_FALSE(precond.IsAllowed());
  EXPECT_EQ(FeaturePromoResult::kError, precond.GetFailure());
}

TEST(CommonPreconditionsTest,
     FeatureEngagementTrackerInitializedPreconditionFailsBeforeInitialization) {
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker, AddOnInitializedCallback);
  FeatureEngagementTrackerInitializedPrecondition precond(&tracker);
  EXPECT_FALSE(precond.IsAllowed());
  EXPECT_EQ(FeaturePromoResult::kBlockedByConfig, precond.GetFailure());
}

TEST(CommonPreconditionsTest,
     FeatureEngagementTrackerInitializedPreconditionFailsAfterInitialization) {
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker, AddOnInitializedCallback)
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(false);
      });
  FeatureEngagementTrackerInitializedPrecondition precond(&tracker);
  EXPECT_FALSE(precond.IsAllowed());
  EXPECT_EQ(FeaturePromoResult::kError, precond.GetFailure());
}

TEST(
    CommonPreconditionsTest,
    FeatureEngagementTrackerInitializedPreconditionSucceedsAfterInitialization) {
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker, AddOnInitializedCallback)
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  FeatureEngagementTrackerInitializedPrecondition precond(&tracker);
  EXPECT_TRUE(precond.IsAllowed());
}

#if !BUILDFLAG(IS_ANDROID)
namespace {
BASE_FEATURE(kTestFeature, "TestFeature", base::FEATURE_ENABLED_BY_DEFAULT);
}

TEST(CommonPreconditionsTest, MeetsFeatureEngagementCriteriaPrecondition) {
  using EventList = feature_engagement::Tracker::EventList;
  using feature_engagement::Comparator;
  using feature_engagement::ComparatorType;
  using feature_engagement::EventConfig;

  feature_engagement::test::MockTracker tracker;
  MeetsFeatureEngagementCriteriaPrecondition precond(kTestFeature, tracker);

  const EventList kPassingEventList{
      {EventConfig("event1", Comparator(ComparatorType::EQUAL, 0), 7, 7), 0},
      {EventConfig("event2", Comparator(ComparatorType::GREATER_THAN, 1), 7, 7),
       2},
  };
  EXPECT_CALL(tracker, ListEvents(testing::Ref(kTestFeature)))
      .WillOnce(testing::Return(kPassingEventList));
  EXPECT_TRUE(precond.IsAllowed());

  const EventList kFailingEventList1{
      {EventConfig("event1", Comparator(ComparatorType::EQUAL, 0), 7, 7), 1},
      {EventConfig("event2", Comparator(ComparatorType::GREATER_THAN, 1), 7, 7),
       2},
  };
  EXPECT_CALL(tracker, ListEvents(testing::Ref(kTestFeature)))
      .WillOnce(testing::Return(kFailingEventList1));
  EXPECT_FALSE(precond.IsAllowed());

  const EventList kFailingEventList2{
      {EventConfig("event1", Comparator(ComparatorType::EQUAL, 0), 7, 7), 0},
      {EventConfig("event2", Comparator(ComparatorType::GREATER_THAN, 1), 7, 7),
       0},
  };
  EXPECT_CALL(tracker, ListEvents(testing::Ref(kTestFeature)))
      .WillOnce(testing::Return(kFailingEventList2));
  EXPECT_FALSE(precond.IsAllowed());
}
#endif

TEST(CommonPreconditionsTest, AnchorElementPrecondition) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestId);
  static const ui::ElementContext kTestContext(1);
  ui::test::TestElement el(kTestId, kTestContext);
  el.Show();

  test::MockAnchorElementProvider provider;
  AnchorElementPrecondition precond(provider, kTestContext);

  EXPECT_CALL(provider, GetAnchorElement(kTestContext))
      .WillOnce(testing::Return(nullptr));
  EXPECT_FALSE(precond.IsAllowed());

  EXPECT_CALL(provider, GetAnchorElement(kTestContext))
      .WillOnce(testing::Return(&el));
  EXPECT_TRUE(precond.IsAllowed());

  EXPECT_CALL(provider, GetAnchorElement(kTestContext))
      .WillOnce(testing::Return(nullptr));
  EXPECT_FALSE(precond.IsAllowed());
}

}  // namespace user_education
