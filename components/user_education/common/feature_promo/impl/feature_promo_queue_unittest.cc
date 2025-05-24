// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/feature_promo_queue.h"

#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_precondition.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/feature_promo/impl/precondition_data.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "components/user_education/test/test_feature_promo_precondition.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"
#include "ui/base/interaction/typed_identifier.h"

namespace user_education::internal {

namespace {

DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kAnchorId);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond1);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond2);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond3);
DEFINE_LOCAL_FEATURE_PROMO_PRECONDITION_IDENTIFIER_VALUE(kPrecond4);
constexpr FeaturePromoResult::Failure kFailure1 =
    FeaturePromoResult::kBlockedByPromo;
constexpr FeaturePromoResult::Failure kFailure2 =
    FeaturePromoResult::kBlockedByConfig;
constexpr FeaturePromoResult::Failure kFailure3 =
    FeaturePromoResult::kBlockedByCooldown;
constexpr FeaturePromoResult::Failure kFailure4 =
    FeaturePromoResult::kBlockedByGracePeriod;
constexpr char kPrecond1Name[] = "Precond1";
constexpr char kPrecond2Name[] = "Precond2";
constexpr char kPrecond3Name[] = "Precond3";
constexpr char kPrecond4Name[] = "Precond4";
constexpr base::TimeDelta kDefaultTimeout = base::Seconds(15);
BASE_FEATURE(kTestFeature1, "TestFeature1", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature2, "TestFeature2", base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kTestFeature3, "TestFeature3", base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

using ResultCallback = FeaturePromoController::ShowPromoResultCallback;

class FeaturePromoQueueTest : public testing::Test {
 public:
  FeaturePromoQueueTest()
      : promo_specs_({
            FeaturePromoSpecification::CreateForTesting(kTestFeature1,
                                                        kAnchorId,
                                                        IDS_OK),
            FeaturePromoSpecification::CreateForTesting(kTestFeature2,
                                                        kAnchorId,
                                                        IDS_OK),
            FeaturePromoSpecification::CreateForTesting(kTestFeature3,
                                                        kAnchorId,
                                                        IDS_OK),
        }) {
    time_provider_.set_clock_for_testing(task_environment_.GetMockClock());
    required_provider_.Add(kPrecond1, kPrecond1Name,
                           FeaturePromoResult::Success());
    required_provider_.Add(kPrecond2, kPrecond2Name,
                           FeaturePromoResult::Success());
    wait_for_provider_.Add(kPrecond3, kPrecond3Name,
                           FeaturePromoResult::Success());
    wait_for_provider_.Add(kPrecond4, kPrecond4Name,
                           FeaturePromoResult::Success());
  }
  ~FeaturePromoQueueTest() override = default;

  FeaturePromoQueue CreateDefaultQueue() const {
    return FeaturePromoQueue(required_provider_, wait_for_provider_,
                             time_provider_, kDefaultTimeout);
  }

  void TryToQueue(FeaturePromoQueue& queue,
                  int which,
                  ResultCallback callback = base::DoNothing()) {
    FeaturePromoParams params(*promo_specs_[which].feature());
    params.show_promo_result_callback = std::move(callback);
    queue.TryToQueue(promo_specs_[which], std::move(params));
  }

  static void RemoveTimedOutPromos(FeaturePromoQueue& queue) {
    FeaturePromoQueue::ComputedDataMap data;
    for (const auto& promo : queue.queued_promos_) {
      data.emplace(&promo.params.feature.get(),
                   FeaturePromoQueue::ComputedData());
    }
    queue.RemoveTimedOutPromos(data);
  }

  static void RemovePromosWithFailedPreconditions(FeaturePromoQueue& queue) {
    queue.RemovePromosWithFailedPreconditions();
  }

  static const base::Feature* IdentifyNextEligiblePromo(
      FeaturePromoQueue& queue) {
    FeaturePromoQueue::ComputedDataMap data;
    for (const auto& promo : queue.queued_promos_) {
      data.emplace(&promo.params.feature.get(),
                   FeaturePromoQueue::ComputedData());
    }
    return queue.IdentifyNextEligiblePromo(data);
  }

  static std::optional<EligibleFeaturePromo> UpdateAndGetNextEligiblePromo(
      FeaturePromoQueue& queue) {
    const base::Feature* const feature =
        queue.UpdateAndIdentifyNextEligiblePromo();
    return feature ? std::make_optional(queue.UnqueueEligiblePromo(*feature))
                   : std::nullopt;
  }

  // Use to verify that a callback *isn't* called.
  // Use EXPECT_ASYNC_CALL_IN_SCOPE() to verify one is.
  void FlushEvents() {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void FastForward(base::TimeDelta amount) {
    task_environment_.FastForwardBy(amount);
  }

  void GetAndCheckNextPromo(FeaturePromoQueue& queue,
                            std::optional<int> index) const {
    const auto result = UpdateAndGetNextEligiblePromo(queue);
    if (index) {
      ASSERT_TRUE(result.has_value());
      const base::Feature* const expected = promo_specs_[*index].feature();
      const base::Feature* const actual = &*result->promo_params.feature;
      EXPECT_EQ(expected, actual) << "Expected feature " << expected->name
                                  << " but got " << actual->name;
    } else {
      EXPECT_FALSE(result.has_value());
    }
  }

  void IdentifyAndCheckNextPromo(FeaturePromoQueue& queue,
                                 std::optional<int> index) const {
    const base::Feature* actual = queue.UpdateAndIdentifyNextEligiblePromo();
    if (index) {
      const base::Feature* const expected = promo_specs_[*index].feature();
      EXPECT_EQ(expected, actual) << "Expected feature " << expected->name
                                  << " but got " << actual->name;
    } else {
      EXPECT_EQ(nullptr, actual);
    }
  }

  test::TestPreconditionListProvider& required() { return required_provider_; }
  test::TestPreconditionListProvider& wait_for() { return wait_for_provider_; }
  const FeaturePromoSpecification& promo_spec(int which) const {
    return promo_specs_[which];
  }
  const UserEducationTimeProvider& time_provider() const {
    return time_provider_;
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  UserEducationTimeProvider time_provider_;
  test::TestPreconditionListProvider required_provider_;
  test::TestPreconditionListProvider wait_for_provider_;
  const std::array<FeaturePromoSpecification, 3> promo_specs_;
};

// Need to test the ability to queue before anything else can be tested.

TEST_F(FeaturePromoQueueTest, TryToQueueSucceeds) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result);
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0, result.Get());
  FlushEvents();
  EXPECT_EQ(1U, queue.queued_count());
  EXPECT_TRUE(queue.IsQueued(kTestFeature1));
  EXPECT_FALSE(queue.IsQueued(kTestFeature2));
  EXPECT_FALSE(queue.is_empty());
}

TEST_F(FeaturePromoQueueTest, TryToQueueFails) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result);
  auto queue = CreateDefaultQueue();
  required().SetDefault(kPrecond1, kFailure1);
  EXPECT_ASYNC_CALL_IN_SCOPE(result, Run(FeaturePromoResult(kFailure1)),
                             TryToQueue(queue, 0, result.Get()));
  EXPECT_EQ(0U, queue.queued_count());
  EXPECT_FALSE(queue.IsQueued(kTestFeature1));
  EXPECT_FALSE(queue.IsQueued(kTestFeature2));
  EXPECT_TRUE(queue.is_empty());
}

TEST_F(FeaturePromoQueueTest, CanQueue) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result);
  auto queue = CreateDefaultQueue();

  // Verify that required conditions affect CanQueue().
  EXPECT_EQ(FeaturePromoResult::Success(),
            queue.CanQueue(promo_spec(0), kTestFeature1));
  required().SetDefault(kPrecond1, kFailure1);
  EXPECT_EQ(FeaturePromoResult(kFailure1),
            queue.CanQueue(promo_spec(0), kTestFeature1));

  // Verify that wait-for conditions do not affect CanQueue().
  wait_for().SetDefault(kPrecond3, kFailure3);
  EXPECT_EQ(FeaturePromoResult(kFailure1),
            queue.CanShow(promo_spec(0), kTestFeature1));
  required().SetDefault(kPrecond1, FeaturePromoResult::Success());
  EXPECT_EQ(FeaturePromoResult::Success(),
            queue.CanQueue(promo_spec(0), kTestFeature1));
}

TEST_F(FeaturePromoQueueTest, CanShow) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result);
  auto queue = CreateDefaultQueue();

  // Verify that required conditions affect CanShow().
  EXPECT_EQ(FeaturePromoResult::Success(),
            queue.CanShow(promo_spec(0), kTestFeature1));
  required().SetDefault(kPrecond1, kFailure1);
  EXPECT_EQ(FeaturePromoResult(kFailure1),
            queue.CanShow(promo_spec(0), kTestFeature1));

  // Verify that required takes precedence over wait-for conditions.
  wait_for().SetDefault(kPrecond3, kFailure3);
  EXPECT_EQ(FeaturePromoResult(kFailure1),
            queue.CanShow(promo_spec(0), kTestFeature1));

  // Verify that wait-for conditions can still affect CanShow().
  required().SetDefault(kPrecond1, FeaturePromoResult::Success());
  EXPECT_EQ(FeaturePromoResult(kFailure3),
            queue.CanShow(promo_spec(0), kTestFeature1));
}

TEST_F(FeaturePromoQueueTest, TryToRequeueFails) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result);
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0, result.Get());
  FlushEvents();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      result, Run(FeaturePromoResult(FeaturePromoResult::kAlreadyQueued)),
      TryToQueue(queue, 0, result.Get()));
  EXPECT_EQ(1U, queue.queued_count());
}

// The following tests check individual private members of FeaturePromoQueue.

TEST_F(FeaturePromoQueueTest, TimeOut) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, result2);
  auto queue = CreateDefaultQueue();
  // Queued at t=0
  TryToQueue(queue, 0, result1.Get());
  FastForward(base::Seconds(10));
  // Queued at t=10
  TryToQueue(queue, 1, result2.Get());
  FastForward(base::Seconds(10));
  // Check at t=20
  EXPECT_ASYNC_CALL_IN_SCOPE(
      result1, Run(FeaturePromoResult(FeaturePromoResult::kTimedOut)),
      RemoveTimedOutPromos(queue));
  EXPECT_EQ(1U, queue.queued_count());
  EXPECT_FALSE(queue.IsQueued(kTestFeature1));
  EXPECT_TRUE(queue.IsQueued(kTestFeature2));
  FastForward(base::Seconds(10));
  // Check at t=30
  EXPECT_ASYNC_CALL_IN_SCOPE(
      result2, Run(FeaturePromoResult(FeaturePromoResult::kTimedOut)),
      RemoveTimedOutPromos(queue));
  EXPECT_EQ(0U, queue.queued_count());
  EXPECT_FALSE(queue.IsQueued(kTestFeature1));
  EXPECT_FALSE(queue.IsQueued(kTestFeature2));
}

TEST_F(FeaturePromoQueueTest, TimeOutRecordsHistograms) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result1);
  auto queue = CreateDefaultQueue();
  // Queued at t=0
  TryToQueue(queue, 0, result1.Get());
  FastForward(base::Seconds(20));
  // Check at t=20
  EXPECT_ASYNC_CALL_IN_SCOPE(result1, Run, RemoveTimedOutPromos(queue));
  histogram_tester_.ExpectTotalCount("UserEducation.MessageShown.TimeInQueue",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.MessageNotShown.TimeInQueue", 1);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.MessageShown.TimeInQueue.TestFeature1", 0);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.MessageNotShown.TimeInQueue.TestFeature1", 1);
}

TEST_F(FeaturePromoQueueTest, FailedRequirements) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, result2);
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0, result1.Get());
  TryToQueue(queue, 1, result2.Get());
  required().SetDefault(kPrecond2, kFailure2);
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(result1, Run(FeaturePromoResult(kFailure2)),
                                result2, Run(FeaturePromoResult(kFailure2)),
                                RemovePromosWithFailedPreconditions(queue));
  EXPECT_EQ(0U, queue.queued_count());
}

TEST_F(FeaturePromoQueueTest, FailedRequirementsRecordsHistograms) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result1);
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0, result1.Get());
  required().SetDefault(kPrecond2, kFailure2);
  EXPECT_ASYNC_CALL_IN_SCOPE(result1, Run,
                             RemovePromosWithFailedPreconditions(queue));
  histogram_tester_.ExpectTotalCount("UserEducation.MessageShown.TimeInQueue",
                                     0);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.MessageNotShown.TimeInQueue", 1);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.MessageShown.TimeInQueue.TestFeature1", 0);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.MessageNotShown.TimeInQueue.TestFeature1", 1);
}

TEST_F(FeaturePromoQueueTest, FailedRequirementOnePromo) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, result2);
  UNCALLED_MOCK_CALLBACK(ResultCallback, result3);
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0, result1.Get());
  TryToQueue(queue, 1, result2.Get());
  TryToQueue(queue, 2, result3.Get());
  required().SetForFeature(kTestFeature2, kPrecond2, kFailure2);
  EXPECT_ASYNC_CALL_IN_SCOPE(result2, Run(FeaturePromoResult(kFailure2)),
                             RemovePromosWithFailedPreconditions(queue));
  EXPECT_EQ(2U, queue.queued_count());
}

TEST_F(FeaturePromoQueueTest, FailedDifferentRequirementsDifferentPromos) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, result2);
  UNCALLED_MOCK_CALLBACK(ResultCallback, result3);
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0, result1.Get());
  TryToQueue(queue, 1, result2.Get());
  TryToQueue(queue, 2, result3.Get());
  required().SetForFeature(kTestFeature2, kPrecond2, kFailure2);
  required().SetForFeature(kTestFeature3, kPrecond1, kFailure1);
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(result2, Run(FeaturePromoResult(kFailure2)),
                                result3, Run(FeaturePromoResult(kFailure1)),
                                RemovePromosWithFailedPreconditions(queue));
  EXPECT_EQ(1U, queue.queued_count());
}

TEST_F(FeaturePromoQueueTest, GetNextEligiblePromo) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result1);
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0, result1.Get());

  (void)UpdateAndGetNextEligiblePromo(queue);
  histogram_tester_.ExpectTotalCount("UserEducation.MessageShown.TimeInQueue",
                                     1);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.MessageNotShown.TimeInQueue", 0);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.MessageShown.TimeInQueue.TestFeature1", 1);
  histogram_tester_.ExpectTotalCount(
      "UserEducation.MessageNotShown.TimeInQueue.TestFeature1", 0);
}

TEST_F(FeaturePromoQueueTest, GetNextEligiblePromoSkipsWaitFor) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, result2);
  UNCALLED_MOCK_CALLBACK(ResultCallback, result3);
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0, result1.Get());
  TryToQueue(queue, 1, result2.Get());
  TryToQueue(queue, 2, result3.Get());

  // This will block the first feature.
  wait_for().SetForFeature(kTestFeature1, kPrecond3, kFailure3);

  EXPECT_EQ(&kTestFeature2, IdentifyNextEligiblePromo(queue));
  const auto promo2 = UpdateAndGetNextEligiblePromo(queue);
  ASSERT_TRUE(promo2.has_value());
  EXPECT_EQ(&kTestFeature2, &promo2->promo_params.feature.get());
  EXPECT_EQ(2U, queue.queued_count());

  EXPECT_EQ(&kTestFeature3, IdentifyNextEligiblePromo(queue));
  const auto promo3 = UpdateAndGetNextEligiblePromo(queue);
  ASSERT_TRUE(promo3.has_value());
  EXPECT_EQ(&kTestFeature3, &promo3->promo_params.feature.get());
  EXPECT_EQ(1U, queue.queued_count());

  EXPECT_EQ(nullptr, IdentifyNextEligiblePromo(queue));
  EXPECT_EQ(std::nullopt, UpdateAndGetNextEligiblePromo(queue));
}

// The following tests check public members of FeaturePromoQueue.

TEST_F(FeaturePromoQueueTest, FailAll) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, result2);
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0, result1.Get());
  TryToQueue(queue, 1, result2.Get());
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(result1, Run(FeaturePromoResult(kFailure4)),
                                result2, Run(FeaturePromoResult(kFailure4)),
                                queue.FailAll(kFailure4));
  EXPECT_EQ(0U, queue.queued_count());
}

TEST_F(FeaturePromoQueueTest, RemoveIneligiblePromos) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result1);
  UNCALLED_MOCK_CALLBACK(ResultCallback, result2);
  UNCALLED_MOCK_CALLBACK(ResultCallback, result3);
  auto queue = CreateDefaultQueue();
  // Queued at t=0
  TryToQueue(queue, 0, result1.Get());
  FastForward(base::Seconds(10));
  // Queued at t=10
  TryToQueue(queue, 1, result2.Get());
  TryToQueue(queue, 2, result3.Get());
  FastForward(base::Seconds(10));
  // Now at t=20. Also, the third promo fails preconditions.
  required().SetForFeature(kTestFeature3, kPrecond1, kFailure1);
  EXPECT_ASYNC_CALLS_IN_SCOPE_2(
      result1, Run(FeaturePromoResult(FeaturePromoResult::kTimedOut)), result3,
      Run(FeaturePromoResult(kFailure1)), queue.RemoveIneligiblePromos());
  EXPECT_EQ(1U, queue.queued_count());
}

TEST_F(FeaturePromoQueueTest, UpdateAndGetNextEligiblePromo_OnePromo) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result1);
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0, result1.Get());
  wait_for().SetForFeature(kTestFeature1, kPrecond3, kFailure3);
  GetAndCheckNextPromo(queue, std::nullopt);
  EXPECT_EQ(1U, queue.queued_count());
  wait_for().SetForFeature(kTestFeature1, kPrecond3,
                           FeaturePromoResult::Success());
  GetAndCheckNextPromo(queue, 0);
  EXPECT_EQ(0U, queue.queued_count());
}

TEST_F(FeaturePromoQueueTest,
       UpdateAndGetNextEligiblePromo_MultiplePromosOutOfOrder) {
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0);
  TryToQueue(queue, 1);
  TryToQueue(queue, 2);
  // Block the second feature; the first and third should be ready to go.
  wait_for().SetForFeature(kTestFeature2, kPrecond3, kFailure3);
  GetAndCheckNextPromo(queue, 0);
  GetAndCheckNextPromo(queue, 2);
  // Once the second is unblocked, it should also be able to go.
  wait_for().SetForFeature(kTestFeature2, kPrecond3,
                           FeaturePromoResult::Success());
  GetAndCheckNextPromo(queue, 1);
}

TEST_F(FeaturePromoQueueTest,
       UpdateAndGetNextEligiblePromo_MultiplePromosDifferentOrder) {
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0);
  TryToQueue(queue, 1);
  TryToQueue(queue, 2);
  // Block the second feature; the first should be ready to go.
  wait_for().SetForFeature(kTestFeature2, kPrecond3, kFailure3);
  GetAndCheckNextPromo(queue, 0);
  // Once the second is unblocked, it should also be able to go.
  wait_for().SetForFeature(kTestFeature2, kPrecond3,
                           FeaturePromoResult::Success());
  GetAndCheckNextPromo(queue, 1);
  GetAndCheckNextPromo(queue, 2);
}

TEST_F(FeaturePromoQueueTest, UpdateAndGetNextEligiblePromo_SomePromosFail) {
  auto queue = CreateDefaultQueue();
  // Queued at t=0
  TryToQueue(queue, 0);
  FastForward(base::Seconds(10));
  // Queued at t=10
  TryToQueue(queue, 1);
  TryToQueue(queue, 2);

  FastForward(base::Seconds(10));
  // Now at t=20, then fail the second feature.
  required().SetForFeature(kTestFeature2, kPrecond1, kFailure1);
  GetAndCheckNextPromo(queue, 2);
  EXPECT_TRUE(queue.is_empty());
}

TEST_F(FeaturePromoQueueTest, QueueRequeueAndRetrievePromosOverTime) {
  auto queue = CreateDefaultQueue();
  // Queued at t=0
  TryToQueue(queue, 0);
  FastForward(base::Seconds(10));
  // Queued at t=10
  TryToQueue(queue, 1);
  TryToQueue(queue, 2);
  EXPECT_TRUE(queue.IsQueued(kTestFeature1));
  EXPECT_TRUE(queue.IsQueued(kTestFeature2));
  EXPECT_TRUE(queue.IsQueued(kTestFeature3));

  FastForward(base::Seconds(10));
  // Now at t=20.
  GetAndCheckNextPromo(queue, 1);
  EXPECT_FALSE(queue.IsQueued(kTestFeature1));
  EXPECT_FALSE(queue.IsQueued(kTestFeature2));
  EXPECT_TRUE(queue.IsQueued(kTestFeature3));

  TryToQueue(queue, 0);
  EXPECT_TRUE(queue.IsQueued(kTestFeature1));
  FastForward(base::Seconds(10));
  // Queued at t=30
  GetAndCheckNextPromo(queue, 0);
  EXPECT_TRUE(queue.is_empty());
}

TEST_F(FeaturePromoQueueTest, UpdateAndIdentifyNextEligiblePromo_OnePromo) {
  UNCALLED_MOCK_CALLBACK(ResultCallback, result1);
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0, result1.Get());
  wait_for().SetForFeature(kTestFeature1, kPrecond3, kFailure3);
  IdentifyAndCheckNextPromo(queue, std::nullopt);
  EXPECT_EQ(1U, queue.queued_count());
  wait_for().SetForFeature(kTestFeature1, kPrecond3,
                           FeaturePromoResult::Success());
  IdentifyAndCheckNextPromo(queue, 0);
  EXPECT_EQ(1U, queue.queued_count());
  queue.FailAll(FeaturePromoResult::kCanceled);
  IdentifyAndCheckNextPromo(queue, std::nullopt);
}

TEST_F(FeaturePromoQueueTest,
       UpdateAndIdentifyNextEligiblePromo_MultiplePromosOutOfOrder) {
  auto queue = CreateDefaultQueue();
  TryToQueue(queue, 0);
  TryToQueue(queue, 1);
  // Block the first feature; the second should be ready to go.
  wait_for().SetForFeature(kTestFeature1, kPrecond3, kFailure3);
  IdentifyAndCheckNextPromo(queue, 1);
  // Once the second is unblocked, it should also be able to go.
  wait_for().SetForFeature(kTestFeature1, kPrecond3,
                           FeaturePromoResult::Success());
  IdentifyAndCheckNextPromo(queue, 0);
}

TEST_F(FeaturePromoQueueTest,
       UpdateAndIdentifyNextEligiblePromo_SomePromosFail) {
  auto queue = CreateDefaultQueue();
  // Queued at t=0
  TryToQueue(queue, 0);
  FastForward(base::Seconds(10));
  // Queued at t=10
  TryToQueue(queue, 1);
  TryToQueue(queue, 2);

  FastForward(base::Seconds(10));
  // Now at t=20, then fail the second feature.
  required().SetForFeature(kTestFeature2, kPrecond1, kFailure1);
  IdentifyAndCheckNextPromo(queue, 2);
  EXPECT_FALSE(queue.is_empty());
}

class FeaturePromoQueueCachedDataTest : public FeaturePromoQueueTest {
 public:
  DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(int, kIntegerValue);
  DECLARE_CLASS_TYPED_IDENTIFIER_VALUE(std::string, kStringValue);

  FeaturePromoQueueCachedDataTest() = default;
  ~FeaturePromoQueueCachedDataTest() override = default;

  template <typename T, typename U>
  static std::unique_ptr<CachingFeaturePromoPrecondition> CreatePrecondition(
      FeaturePromoPrecondition::Identifier id,
      FeaturePromoResult::Failure failure,
      std::string name,
      ui::TypedIdentifier<T> key,
      U data) {
    auto precond = std::make_unique<CachingFeaturePromoPrecondition>(
        kPrecond1, kPrecond1Name, FeaturePromoResult::Success());
    precond->InitCachedData(key, std::forward<U>(data));
    return precond;
  }
};

DEFINE_CLASS_TYPED_IDENTIFIER_VALUE(FeaturePromoQueueCachedDataTest,
                                    int,
                                    kIntegerValue);
DEFINE_CLASS_TYPED_IDENTIFIER_VALUE(FeaturePromoQueueCachedDataTest,
                                    std::string,
                                    kStringValue);

TEST_F(FeaturePromoQueueCachedDataTest, ExtractsCachedData) {
  test::MockPreconditionListProvider required_preconditions;
  test::MockPreconditionListProvider wait_for_preconditions;

  EXPECT_CALL(required_preconditions, GetPreconditions)
      .WillRepeatedly([](const FeaturePromoSpecification&,
                         const FeaturePromoParams&) {
        FeaturePromoPreconditionList list;
        list.AddPrecondition(CreatePrecondition(
            kPrecond1, kFailure1, kPrecond1Name, kIntegerValue, 2));
        return list;
      });
  EXPECT_CALL(wait_for_preconditions, GetPreconditions)
      .WillRepeatedly([](const FeaturePromoSpecification&,
                         const FeaturePromoParams&) {
        FeaturePromoPreconditionList list;
        list.AddPrecondition(CreatePrecondition(
            kPrecond2, kFailure2, kPrecond2Name, kStringValue, "foo"));
        return list;
      });

  FeaturePromoQueue queue(required_preconditions, wait_for_preconditions,
                          time_provider(), base::Seconds(10));
  queue.TryToQueue(promo_spec(0), {kTestFeature1});
  auto result = UpdateAndGetNextEligiblePromo(queue);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(&kTestFeature1, &*result->promo_params.feature);
  EXPECT_EQ(2, *PreconditionData::Get(result->cached_data, kIntegerValue));
  EXPECT_EQ("foo", *PreconditionData::Get(result->cached_data, kStringValue));

  EXPECT_FALSE(UpdateAndGetNextEligiblePromo(queue).has_value());
}

}  // namespace user_education::internal
