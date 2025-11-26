// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/feature_promo/impl/messaging_coordinator.h"

#include <optional>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/test/test_product_messaging_controller.h"
#include "components/user_education/test/test_user_education_storage_service.h"
#include "components/user_education/test/user_education_session_mocks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"

namespace user_education::internal {

namespace {

DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(kNoticeId);

using PromoState = MessagingCoordinator::PromoState;

}  // namespace

class MessagingCoordinatorTest : public testing::Test {
 public:
  MessagingCoordinatorTest() = default;
  ~MessagingCoordinatorTest() override = default;

  void SetUp() override {
    controller_.Init(session_provider_, storage_service_);
  }

  ProductMessagingController& controller() { return controller_; }

  void FlushEvents() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void ExpectRequestPending(bool low, bool high) {
    EXPECT_EQ(low, controller_.IsNoticeQueued(kLowPriorityNoticeId));
    EXPECT_EQ(high, controller_.IsNoticeQueued(kHighPriorityNoticeId));
  }

  // Since this is a friend class of the coordinator, it can access these ids.
  const RequiredNoticeId kLowPriorityNoticeId =
      MessagingCoordinator::kLowPriorityNoticeId;
  const RequiredNoticeId kHighPriorityNoticeId =
      MessagingCoordinator::kHighPriorityNoticeId;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::TestUserEducationSessionProvider session_provider_{false};
  test::TestUserEducationStorageService storage_service_;
  ProductMessagingController controller_;
};

// Creates mock callbacks and a coordinator with some default names as local
// variables.
#define DECLARE_LOCALS()                                              \
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, ready);              \
  UNCALLED_MOCK_CALLBACK(base::RepeatingClosure, preempted);          \
  MessagingCoordinator coordinator(controller());                     \
  const auto __sub1 = coordinator.AddPromoReadyCallback(ready.Get()); \
  const auto __sub2 = coordinator.AddPromoPreemptedCallback(preempted.Get())

#define EXPECT_STATE(State, LowPending, HighPending, LowAllowed, HighAllowed) \
  EXPECT_EQ(State, coordinator.promo_state_for_testing());                    \
  ExpectRequestPending(LowPending, HighPending);                              \
  EXPECT_EQ(LowAllowed, coordinator.CanShowPromo(false));                     \
  EXPECT_EQ(HighAllowed, coordinator.CanShowPromo(true))

TEST_F(MessagingCoordinatorTest, InitialState) {
  DECLARE_LOCALS();
  EXPECT_STATE(PromoState::kNone, false, false, false, false);
}

TEST_F(MessagingCoordinatorTest, TransitionToLowPriorityPending) {
  DECLARE_LOCALS();
  coordinator.TransitionToState(PromoState::kLowPriorityPending);
  EXPECT_STATE(PromoState::kLowPriorityPending, true, false, false, false);
  EXPECT_CALL_IN_SCOPE(ready, Run, FlushEvents());
  EXPECT_STATE(PromoState::kLowPriorityPending, false, false, true, true);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToLowPriorityPendingAgainBeforeReady) {
  DECLARE_LOCALS();
  coordinator.TransitionToState(PromoState::kLowPriorityPending);

  coordinator.TransitionToState(PromoState::kLowPriorityPending);
  EXPECT_STATE(PromoState::kLowPriorityPending, true, false, false, false);
  EXPECT_CALL_IN_SCOPE(ready, Run, FlushEvents());
  EXPECT_STATE(PromoState::kLowPriorityPending, false, false, true, true);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToLowPriorityPendingAgainAfterReadyFails) {
  DECLARE_LOCALS();
  coordinator.TransitionToState(PromoState::kLowPriorityPending);
  EXPECT_CALL_IN_SCOPE(ready, Run, FlushEvents());

  EXPECT_CHECK_DEATH(
      coordinator.TransitionToState(PromoState::kLowPriorityPending));
}

TEST_F(MessagingCoordinatorTest, TransitionToHighPriorityPending) {
  DECLARE_LOCALS();
  coordinator.TransitionToState(PromoState::kHighPriorityPending);
  EXPECT_STATE(PromoState::kHighPriorityPending, false, true, false, false);
  EXPECT_CALL_IN_SCOPE(ready, Run, FlushEvents());
  EXPECT_STATE(PromoState::kHighPriorityPending, false, false, false, true);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToHighPriorityPendingAgainBeforeReady) {
  DECLARE_LOCALS();
  coordinator.TransitionToState(PromoState::kHighPriorityPending);

  coordinator.TransitionToState(PromoState::kHighPriorityPending);
  EXPECT_STATE(PromoState::kHighPriorityPending, false, true, false, false);
  EXPECT_CALL_IN_SCOPE(ready, Run, FlushEvents());
  EXPECT_STATE(PromoState::kHighPriorityPending, false, false, false, true);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToHighPriorityPendingAgainAfterReadyFails) {
  DECLARE_LOCALS();
  coordinator.TransitionToState(PromoState::kHighPriorityPending);
  EXPECT_CALL_IN_SCOPE(ready, Run, FlushEvents());
  EXPECT_CHECK_DEATH(
      coordinator.TransitionToState(PromoState::kHighPriorityPending));
}

TEST_F(MessagingCoordinatorTest,
       TransitionToLowPriorityShowingFromLowPriorityPending) {
  DECLARE_LOCALS();
  coordinator.TransitionToState(PromoState::kLowPriorityPending);
  EXPECT_CALL_IN_SCOPE(ready, Run, FlushEvents());
  coordinator.TransitionToState(PromoState::kLowPriorityShowing);
  EXPECT_STATE(PromoState::kLowPriorityShowing, false, false, false, false);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToHighPriorityShowingFromHighPriorityPending) {
  DECLARE_LOCALS();
  coordinator.TransitionToState(PromoState::kHighPriorityPending);
  EXPECT_CALL_IN_SCOPE(ready, Run, FlushEvents());
  coordinator.TransitionToState(PromoState::kHighPriorityShowing);
  EXPECT_STATE(PromoState::kHighPriorityShowing, false, false, false, true);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToHighPriorityShowingFromLowPriorityPending) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kLowPriorityPending));
  coordinator.TransitionToState(PromoState::kHighPriorityShowing);
  EXPECT_STATE(PromoState::kHighPriorityShowing, false, false, false, true);
}

TEST_F(MessagingCoordinatorTest, TransitionToLowPriorityShowingFromNoneFails) {
  DECLARE_LOCALS();
  EXPECT_CHECK_DEATH(
      coordinator.TransitionToState(PromoState::kLowPriorityShowing));
}

TEST_F(MessagingCoordinatorTest, TransitionToHighPriorityShowingFromNoneFails) {
  DECLARE_LOCALS();
  EXPECT_CHECK_DEATH(
      coordinator.TransitionToState(PromoState::kHighPriorityShowing));
}

TEST_F(MessagingCoordinatorTest,
       TransitionToLowPriorityShowingFromHighPriorityPendingFails) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kHighPriorityPending));
  EXPECT_CHECK_DEATH(
      coordinator.TransitionToState(PromoState::kLowPriorityShowing));
}

TEST_F(MessagingCoordinatorTest,
       TransitionToLowPriorityShowingFromLowPriorityShowingFails) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kLowPriorityPending));
  coordinator.TransitionToState(PromoState::kLowPriorityShowing);
  FlushEvents();
  // Can't re-show low priority promo.
  EXPECT_CHECK_DEATH(
      coordinator.TransitionToState(PromoState::kLowPriorityShowing));
}

TEST_F(MessagingCoordinatorTest,
       TransitionToHighPriorityShowingFromLowPriorityShowingFails) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kLowPriorityPending));
  coordinator.TransitionToState(PromoState::kLowPriorityShowing);
  FlushEvents();
  // Can't just go to high priority promo showing.
  EXPECT_CHECK_DEATH(
      coordinator.TransitionToState(PromoState::kHighPriorityShowing));
}

TEST_F(MessagingCoordinatorTest,
       TransitionToHighPriorityShowingFromHighPriorityShowing) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kHighPriorityPending));
  coordinator.TransitionToState(PromoState::kHighPriorityShowing);
  FlushEvents();
  coordinator.TransitionToState(PromoState::kHighPriorityShowing);
  EXPECT_STATE(PromoState::kHighPriorityShowing, false, false, false, true);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToLowPriorityPendingFromHighPriorityPendingBeforeReady) {
  DECLARE_LOCALS();
  coordinator.TransitionToState(PromoState::kHighPriorityPending);
  ExpectRequestPending(false, true);

  // This should cause release queue position and re-request the handle.
  coordinator.TransitionToState(PromoState::kLowPriorityPending);
  EXPECT_STATE(PromoState::kLowPriorityPending, true, false, false, false);
  EXPECT_CALL_IN_SCOPE(ready, Run, FlushEvents());
  EXPECT_STATE(PromoState::kLowPriorityPending, false, false, true, true);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToLowPriorityPendingFromHighPriorityPendingWhenReady) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kHighPriorityPending));

  // This should cause release and re-request the handle.
  coordinator.TransitionToState(PromoState::kLowPriorityPending);
  EXPECT_STATE(PromoState::kLowPriorityPending, true, false, false, false);
  EXPECT_CALL_IN_SCOPE(ready, Run, FlushEvents());
  EXPECT_STATE(PromoState::kLowPriorityPending, false, false, true, true);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToHighPriorityPendingFromLowPriorityPendingBeforeReady) {
  DECLARE_LOCALS();
  coordinator.TransitionToState(PromoState::kLowPriorityPending);
  ExpectRequestPending(true, false);

  // This should cause release queue position and re-request the handle.
  coordinator.TransitionToState(PromoState::kHighPriorityPending);
  EXPECT_STATE(PromoState::kHighPriorityPending, false, true, false, false);
  EXPECT_CALL_IN_SCOPE(ready, Run, FlushEvents());
  EXPECT_STATE(PromoState::kHighPriorityPending, false, false, false, true);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToHighPriorityPendingFromLowPriorityPendingWhenReadyFails) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kLowPriorityPending));
  EXPECT_CHECK_DEATH(
      coordinator.TransitionToState(PromoState::kHighPriorityPending));
}

TEST_F(MessagingCoordinatorTest,
       TransitionToNoneFromLowPriorityPendingBeforeReady) {
  DECLARE_LOCALS();
  coordinator.TransitionToState(PromoState::kLowPriorityPending);
  coordinator.TransitionToState(PromoState::kNone);
  EXPECT_STATE(PromoState::kNone, false, false, false, false);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToNoneFromLowPriorityPendingWhenReady) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kLowPriorityPending));
  coordinator.TransitionToState(PromoState::kNone);
  EXPECT_STATE(PromoState::kNone, false, false, false, false);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToNoneFromHighPriorityPendingBeforeReady) {
  DECLARE_LOCALS();
  coordinator.TransitionToState(PromoState::kHighPriorityPending);
  coordinator.TransitionToState(PromoState::kNone);
  EXPECT_STATE(PromoState::kNone, false, false, false, false);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToNoneFromHighPriorityPendingWhenReady) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kHighPriorityPending));
  coordinator.TransitionToState(PromoState::kNone);
  EXPECT_STATE(PromoState::kNone, false, false, false, false);
}

TEST_F(MessagingCoordinatorTest, TransitionToNoneFromLowPriorityShowing) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kLowPriorityPending));
  coordinator.TransitionToState(PromoState::kLowPriorityShowing);
  coordinator.TransitionToState(PromoState::kNone);
  EXPECT_STATE(PromoState::kNone, false, false, false, false);
}

TEST_F(MessagingCoordinatorTest, TransitionToNoneFromHighPriorityShowing) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kHighPriorityPending));
  coordinator.TransitionToState(PromoState::kHighPriorityShowing);
  coordinator.TransitionToState(PromoState::kNone);
  EXPECT_STATE(PromoState::kNone, false, false, false, false);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToLowPriorityPendingFromLowPriorityShowing) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kLowPriorityPending));
  coordinator.TransitionToState(PromoState::kLowPriorityShowing);
  coordinator.TransitionToState(PromoState::kLowPriorityPending);
  EXPECT_STATE(PromoState::kLowPriorityPending, true, false, false, false);
  EXPECT_ASYNC_CALL_IN_SCOPE(ready, Run, FlushEvents());
  EXPECT_STATE(PromoState::kLowPriorityPending, false, false, true, true);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToLowPriorityPendingFromHighPriorityShowing) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kHighPriorityPending));
  coordinator.TransitionToState(PromoState::kHighPriorityShowing);
  coordinator.TransitionToState(PromoState::kLowPriorityPending);
  EXPECT_STATE(PromoState::kLowPriorityPending, true, false, false, false);
  EXPECT_ASYNC_CALL_IN_SCOPE(ready, Run, FlushEvents());
  EXPECT_STATE(PromoState::kLowPriorityPending, false, false, true, true);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToHighPriorityPendingFromLowPriorityShowing) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kLowPriorityPending));
  coordinator.TransitionToState(PromoState::kLowPriorityShowing);
  coordinator.TransitionToState(PromoState::kHighPriorityPending);
  EXPECT_STATE(PromoState::kHighPriorityPending, false, true, false, false);
  EXPECT_ASYNC_CALL_IN_SCOPE(ready, Run, FlushEvents());
  EXPECT_STATE(PromoState::kHighPriorityPending, false, false, false, true);
}

TEST_F(MessagingCoordinatorTest,
       TransitionToHighPriorityPendingFromHighPriorityShowingFails) {
  DECLARE_LOCALS();
  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kHighPriorityPending));
  coordinator.TransitionToState(PromoState::kHighPriorityShowing);
  EXPECT_CHECK_DEATH(
      coordinator.TransitionToState(PromoState::kHighPriorityPending));
}

TEST_F(MessagingCoordinatorTest, HandleGrantedToOtherMessageWhileNoActivity) {
  DECLARE_LOCALS();

  // Have a third-party notification claim the handle. No events should be
  // triggered.
  test::TestNotice notice(controller(), kNoticeId);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
}

TEST_F(MessagingCoordinatorTest,
       HandleGrantedToOtherMessageWhileLowPriorityPendingBeforeReady) {
  DECLARE_LOCALS();

  coordinator.TransitionToState(PromoState::kLowPriorityPending);

  // Have a third-party notification claim the handle. This will come before the
  // low priority message in the queue.
  test::TestNotice notice(controller(), kNoticeId);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  EXPECT_STATE(PromoState::kLowPriorityPending, true, false, false, false);
}

TEST_F(MessagingCoordinatorTest,
       HandleNotGrantedToOtherMessageWhileLowPriorityPendingAfterReady) {
  DECLARE_LOCALS();

  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kLowPriorityPending));

  // Have a third-party notification get in queue. Since the handle is already
  // held by the coordinator, it is kept.
  test::TestNotice notice(controller(), kNoticeId);
  EXPECT_FALSE(notice.has_priority());
  EXPECT_STATE(PromoState::kLowPriorityPending, false, false, true, true);

  // Releasing the handle should allow the other notice to show.
  coordinator.TransitionToState(PromoState::kNone);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
}

TEST_F(MessagingCoordinatorTest,
       OtherMessageWhileLowPriorityPendingAfterReadyPreemptsAfterShown) {
  DECLARE_LOCALS();

  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kLowPriorityPending));
  test::TestNotice notice(controller(), kNoticeId);

  // The other notice should seize priority when the handle is released.
  coordinator.TransitionToState(PromoState::kLowPriorityShowing);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  EXPECT_ASYNC_CALL_IN_SCOPE(preempted, Run, notice.SetShown());
  EXPECT_TRUE(notice.has_priority());
}

TEST_F(MessagingCoordinatorTest,
       QueueExternalMessageWhileHighPriorityPendingBeforeReady) {
  DECLARE_LOCALS();

  // Both the high priority and third-party message will contend for the handle.
  // Because the controller is currently *not* deterministic, we simply
  // determine that both are queued and then that one is granted.
  coordinator.TransitionToState(PromoState::kHighPriorityPending);
  test::TestNotice notice(controller(), kNoticeId);

  EXPECT_TRUE(controller().IsNoticeQueued(kNoticeId));
  EXPECT_TRUE(controller().IsNoticeQueued(kHighPriorityNoticeId));

  // This may or may not be called, so make it so it doesn't care.
  EXPECT_CALL(ready, Run).WillRepeatedly([]() {});

  FlushEvents();

  // Either the external message got priority or the coordinator got permission
  // to show the high-priority message, but not both.
  EXPECT_NE(notice.has_priority(), coordinator.CanShowPromo(true));
  EXPECT_NE(controller().IsNoticeQueued(kNoticeId), notice.has_priority());
  EXPECT_NE(controller().IsNoticeQueued(kHighPriorityNoticeId),
            coordinator.CanShowPromo(true));
}

TEST_F(MessagingCoordinatorTest,
       HandleNotGrantedToOtherMessageWhileHighPriorityPendingAfterReady) {
  DECLARE_LOCALS();

  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kHighPriorityPending));

  // Have a third-party notification get in queue. Since the handle is already
  // held by the coordinator, it is kept.
  test::TestNotice notice(controller(), kNoticeId);
  EXPECT_FALSE(notice.has_priority());
  EXPECT_STATE(PromoState::kHighPriorityPending, false, false, false, true);

  // Releasing the handle should allow the other notice to show.
  coordinator.TransitionToState(PromoState::kNone);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
}

TEST_F(MessagingCoordinatorTest,
       OtherMessageWhileHighPriorityPendingAfterReadyDoesNotPreemptAfterShown) {
  DECLARE_LOCALS();

  EXPECT_ASYNC_CALL_IN_SCOPE(
      ready, Run,
      coordinator.TransitionToState(PromoState::kHighPriorityPending));
  test::TestNotice notice(controller(), kNoticeId);
  coordinator.TransitionToState(PromoState::kHighPriorityShowing);
  FlushEvents();
  EXPECT_FALSE(notice.has_priority());

  // Releasing the handle should allow the other notice to show.
  coordinator.TransitionToState(PromoState::kNone);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
}

}  // namespace user_education::internal
