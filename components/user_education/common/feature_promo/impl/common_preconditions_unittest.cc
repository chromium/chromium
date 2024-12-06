// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/common_preconditions.h"

#include <memory>

#include "base/feature_list.h"
#include "base/test/bind.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/user_education/common/feature_promo/feature_promo_lifecycle.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo/impl/precondition_data.h"
#include "components/user_education/test/mock_anchor_element_provider.h"
#include "components/user_education/test/test_user_education_storage_service.h"
#include "components/user_education/test/user_education_session_mocks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_test_util.h"
#include "ui/base/interaction/expect_call_in_scope.h"

namespace user_education {

namespace {
BASE_FEATURE(kTestFeature, "TestFeature", base::FEATURE_ENABLED_BY_DEFAULT);
}

TEST(CommonPreconditionsTest,
     FeatureEngagementTrackerInitializedPreconditionFailsNoTracker) {
  FeatureEngagementTrackerInitializedPrecondition precond(nullptr);
  EXPECT_EQ(FeaturePromoResult::kError, precond.CheckPrecondition());
}

TEST(CommonPreconditionsTest,
     FeatureEngagementTrackerInitializedPreconditionFailsBeforeInitialization) {
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker, AddOnInitializedCallback);
  FeatureEngagementTrackerInitializedPrecondition precond(&tracker);
  EXPECT_EQ(FeaturePromoResult::kBlockedByConfig,
            FeaturePromoResult::kBlockedByConfig);
}

TEST(CommonPreconditionsTest,
     FeatureEngagementTrackerInitializedPreconditionFailsAfterInitialization) {
  feature_engagement::test::MockTracker tracker;
  EXPECT_CALL(tracker, AddOnInitializedCallback)
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(false);
      });
  FeatureEngagementTrackerInitializedPrecondition precond(&tracker);
  EXPECT_EQ(FeaturePromoResult::kError, FeaturePromoResult::kError);
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
  EXPECT_EQ(FeaturePromoResult::Success(), precond.CheckPrecondition());
}

#if !BUILDFLAG(IS_ANDROID)
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
  EXPECT_EQ(FeaturePromoResult::Success(), precond.CheckPrecondition());

  const EventList kFailingEventList1{
      {EventConfig("event1", Comparator(ComparatorType::EQUAL, 0), 7, 7), 1},
      {EventConfig("event2", Comparator(ComparatorType::GREATER_THAN, 1), 7, 7),
       2},
  };
  EXPECT_CALL(tracker, ListEvents(testing::Ref(kTestFeature)))
      .WillOnce(testing::Return(kFailingEventList1));
  EXPECT_EQ(FeaturePromoResult::kBlockedByConfig, precond.CheckPrecondition());

  const EventList kFailingEventList2{
      {EventConfig("event1", Comparator(ComparatorType::EQUAL, 0), 7, 7), 0},
      {EventConfig("event2", Comparator(ComparatorType::GREATER_THAN, 1), 7, 7),
       0},
  };
  EXPECT_CALL(tracker, ListEvents(testing::Ref(kTestFeature)))
      .WillOnce(testing::Return(kFailingEventList2));
  EXPECT_EQ(FeaturePromoResult::kBlockedByConfig, precond.CheckPrecondition());
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST(CommonPreconditionsTest, AnchorElementPrecondition) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestId);
  static const ui::ElementContext kTestContext(1);
  ui::test::TestElement el(kTestId, kTestContext);
  el.Show();

  test::MockAnchorElementProvider provider;
  AnchorElementPrecondition precond(provider, kTestContext);

  EXPECT_CALL(provider, GetAnchorElement(kTestContext))
      .WillOnce(testing::Return(nullptr));
  EXPECT_EQ(FeaturePromoResult::kBlockedByUi, precond.CheckPrecondition());

  EXPECT_CALL(provider, GetAnchorElement(kTestContext))
      .WillOnce(testing::Return(&el));
  EXPECT_EQ(FeaturePromoResult::Success(), precond.CheckPrecondition());

  EXPECT_CALL(provider, GetAnchorElement(kTestContext))
      .WillOnce(testing::Return(nullptr));
  EXPECT_EQ(FeaturePromoResult::kBlockedByUi, precond.CheckPrecondition());
}

TEST(CommonPreconditionsTest,
     AnchorElementPrecondition_ExtractCachedDataReturnsElement) {
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kTestId);
  static const ui::ElementContext kTestContext(1);
  ui::test::TestElement el(kTestId, kTestContext);
  el.Show();

  test::MockAnchorElementProvider provider;
  AnchorElementPrecondition precond(provider, kTestContext);

  EXPECT_CALL(provider, GetAnchorElement(kTestContext))
      .WillOnce(testing::Return(&el));
  EXPECT_EQ(FeaturePromoResult::Success(), precond.CheckPrecondition());

  internal::PreconditionData::Collection coll;
  precond.ExtractCachedData(coll);
  auto* data = internal::PreconditionData::Get(
      coll, AnchorElementPrecondition::kAnchorElement);
  ASSERT_NE(nullptr, data);
  EXPECT_EQ(&el, data->get());
}

TEST(CommonPreconditionsTest,
     AnchorElementPrecondition_ExtractCachedDataReturnsNull) {
  static const ui::ElementContext kTestContext(1);

  test::MockAnchorElementProvider provider;
  AnchorElementPrecondition precond(provider, kTestContext);

  EXPECT_CALL(provider, GetAnchorElement(kTestContext))
      .WillOnce(testing::Return(nullptr));
  EXPECT_EQ(FeaturePromoResult::kBlockedByUi, precond.CheckPrecondition());

  internal::PreconditionData::Collection coll;
  precond.ExtractCachedData(coll);
  auto* data = internal::PreconditionData::Get(
      coll, AnchorElementPrecondition::kAnchorElement);
  ASSERT_NE(nullptr, data);
  EXPECT_EQ(nullptr, data->get());
}

TEST(CommonPreconditionsTest, LifecyclePrecondition) {
  test::TestUserEducationStorageService storage_service;

  auto lifecycle_ptr = std::make_unique<FeaturePromoLifecycle>(
      &storage_service, "", &kTestFeature,
      FeaturePromoLifecycle::PromoType::kToast,
      FeaturePromoLifecycle::PromoSubtype::kNormal, 0);

  LifecyclePrecondition precond(std::move(lifecycle_ptr), /*for_demo=*/false);
  EXPECT_EQ(FeaturePromoResult::Success(), precond.CheckPrecondition());

  FeaturePromoData data;
  data.is_dismissed = true;
  data.show_count = 1;
  storage_service.SavePromoData(kTestFeature, data);

  EXPECT_EQ(FeaturePromoResult::kPermanentlyDismissed,
            precond.CheckPrecondition());
}

TEST(CommonPreconditionsTest, LifecyclePreconditionForDemo) {
  test::TestUserEducationStorageService storage_service;

  auto lifecycle_ptr = std::make_unique<FeaturePromoLifecycle>(
      &storage_service, "", &kTestFeature,
      FeaturePromoLifecycle::PromoType::kToast,
      FeaturePromoLifecycle::PromoSubtype::kNormal, 0);

  LifecyclePrecondition precond(std::move(lifecycle_ptr), /*for_demo=*/true);
  EXPECT_EQ(FeaturePromoResult::Success(), precond.CheckPrecondition());

  FeaturePromoData data;
  data.is_dismissed = true;
  data.show_count = 1;
  storage_service.SavePromoData(kTestFeature, data);

  EXPECT_EQ(FeaturePromoResult::Success(), precond.CheckPrecondition());
}

TEST(CommonPreconditionsTest, SessionPolicyPreconditionSucceeds) {
  UNCALLED_MOCK_CALLBACK(SessionPolicyPrecondition::GetCurrentPromoInfoCallback,
                         get_current);

  test::TestUserEducationStorageService storage_service;
  test::MockFeaturePromoSessionPolicy session_policy;
  session_policy.Init(nullptr, &storage_service);
  FeaturePromoPriorityProvider::PromoPriorityInfo priority_info{
      FeaturePromoPriorityProvider::PromoWeight::kLight,
      FeaturePromoPriorityProvider::PromoPriority::kMedium};
  std::optional<FeaturePromoPriorityProvider::PromoPriorityInfo> last_info(
      FeaturePromoPriorityProvider::PromoPriorityInfo{
          FeaturePromoPriorityProvider::PromoWeight::kHeavy,
          FeaturePromoPriorityProvider::PromoPriority::kHigh});
  EXPECT_CALL(session_policy, CanShowPromo(priority_info, last_info))
      .WillOnce(
          testing::Return(FeaturePromoResult(FeaturePromoResult::Success())));
  EXPECT_CALL(get_current, Run).WillOnce(testing::Return(last_info));

  SessionPolicyPrecondition precond(&session_policy, priority_info,
                                    get_current.Get());
  EXPECT_EQ(FeaturePromoResult::Success(), precond.CheckPrecondition());
}

TEST(CommonPreconditionsTest, SessionPolicyPreconditionFails) {
  UNCALLED_MOCK_CALLBACK(SessionPolicyPrecondition::GetCurrentPromoInfoCallback,
                         get_current);

  test::TestUserEducationStorageService storage_service;
  test::MockFeaturePromoSessionPolicy session_policy;
  session_policy.Init(nullptr, &storage_service);
  FeaturePromoPriorityProvider::PromoPriorityInfo priority_info{
      FeaturePromoPriorityProvider::PromoWeight::kLight,
      FeaturePromoPriorityProvider::PromoPriority::kMedium};
  std::optional<FeaturePromoPriorityProvider::PromoPriorityInfo> last_info(
      FeaturePromoPriorityProvider::PromoPriorityInfo{
          FeaturePromoPriorityProvider::PromoWeight::kHeavy,
          FeaturePromoPriorityProvider::PromoPriority::kHigh});
  EXPECT_CALL(session_policy, CanShowPromo(priority_info, last_info))
      .WillOnce(testing::Return(
          FeaturePromoResult(FeaturePromoResult::kBlockedByCooldown)));
  EXPECT_CALL(get_current, Run).WillOnce(testing::Return(last_info));

  SessionPolicyPrecondition precond(&session_policy, priority_info,
                                    get_current.Get());
  EXPECT_EQ(FeaturePromoResult::kBlockedByCooldown,
            precond.CheckPrecondition());
}

}  // namespace user_education
