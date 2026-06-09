// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/product_messaging_controller.h"

#include <initializer_list>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "components/user_education/common/user_education_data.h"
#include "components/user_education/test/test_product_messaging_controller.h"
#include "components/user_education/test/test_user_education_storage_service.h"
#include "components/user_education/test/user_education_session_mocks.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"

namespace user_education {

namespace {

DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(kNoticeId1,
                                 ProductMessageType::kLegalOrComplianceNotice);
DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(kNoticeId2,
                                 ProductMessageType::kLegalOrComplianceNotice);
DEFINE_LOCAL_PRODUCT_MESSAGE_KEY(kNoticeId3,
                                 ProductMessageType::kLegalOrComplianceNotice);

}  // namespace

class ProductMessagingControllerTest : public testing::Test {
 public:
  ProductMessagingControllerTest() = default;
  ~ProductMessagingControllerTest() override = default;

  void SetUp() override {
    controller_.Init(session_provider_, storage_service_);
  }

  ProductMessagingController& controller() { return controller_; }
  test::TestUserEducationSessionProvider& session_provider() {
    return session_provider_;
  }
  test::TestUserEducationStorageService& storage_service() {
    return storage_service_;
  }

  void FlushEvents() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  ProductMessageKey GetCurrentMessage() const {
    ProductMessageKey found_key;
    for (const auto& [info_key, info_status] :
         controller_.GetAllMessages({ProductMessageStatus::kEligible,
                                     ProductMessageStatus::kShowing})) {
      if (!found_key || info_key.type() > found_key.type()) {
        found_key = info_key;
      }
    }
    return found_key;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::TestUserEducationSessionProvider session_provider_{false};
  test::TestUserEducationStorageService storage_service_;
  ProductMessagingController controller_;
};

TEST_F(ProductMessagingControllerTest, ConditionallyRecordsDone) {
  test::TestProductMessage notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  notice.SetShown();
  notice.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::UnorderedElementsAre(kNoticeId1.GetName()));

  test::TestProductMessage notice2(controller(), kNoticeId2);
  FlushEvents();
  notice2.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::UnorderedElementsAre(kNoticeId1.GetName()));

  test::TestProductMessage notice3(controller(), kNoticeId3);
  FlushEvents();
  notice3.SetShown();
  notice3.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::UnorderedElementsAre(kNoticeId1.GetName(),
                                            kNoticeId3.GetName()));
}

TEST_F(ProductMessagingControllerTest, ShownBlocksSelf) {
  test::TestProductMessage notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  notice.SetShown();
  notice.Release();
  EXPECT_FALSE(notice.has_priority());

  test::TestProductMessage notice2(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_FALSE(notice2.has_priority());
}

TEST_F(ProductMessagingControllerTest, NotShownDoesNotBlockSelf) {
  test::TestProductMessage notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  notice.Release();
  EXPECT_FALSE(notice.has_priority());

  test::TestProductMessage notice2(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  EXPECT_FALSE(notice2.has_priority());
}

TEST_F(ProductMessagingControllerTest, ClearsOnNewSession) {
  test::TestProductMessage notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.has_priority());
  notice.SetShown();
  notice.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::UnorderedElementsAre(kNoticeId1.GetName()));
  session_provider().StartNewSession();
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::IsEmpty());
}

TEST_F(ProductMessagingControllerTest, ClearsOnNewSessionAtProgramStart) {
  ProductMessagingController controller;
  test::TestUserEducationStorageService storage_service;
  ProductMessagingData data;
  data.shown_notices.insert(kNoticeId1.GetName());
  data.shown_notices.insert(kNoticeId2.GetName());
  storage_service.SaveProductMessagingData(data);
  EXPECT_FALSE(
      storage_service.ReadProductMessagingData().shown_notices.empty());

  test::TestUserEducationSessionProvider session_provider(true);
  controller.Init(session_provider, storage_service);

  EXPECT_TRUE(storage_service.ReadProductMessagingData().shown_notices.empty());
}

TEST_F(ProductMessagingControllerTest,
       DoesNotClearIfNoNewSessionAtProgramStart) {
  ProductMessagingController controller;
  test::TestUserEducationStorageService storage_service;
  ProductMessagingData data;
  data.shown_notices.insert(kNoticeId1.GetName());
  data.shown_notices.insert(kNoticeId2.GetName());
  storage_service.SaveProductMessagingData(data);
  EXPECT_FALSE(
      storage_service.ReadProductMessagingData().shown_notices.empty());

  test::TestUserEducationSessionProvider session_provider(false);
  controller.Init(session_provider, storage_service);

  EXPECT_FALSE(
      storage_service.ReadProductMessagingData().shown_notices.empty());
}

TEST_F(ProductMessagingControllerTest, QueueAndShowSingleNotice) {
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
  EXPECT_EQ(ProductMessageKey(), GetCurrentMessage());
  test::TestProductMessage notice(controller(), kNoticeId1);
  EXPECT_TRUE(controller().HasPendingMessagesForTesting());
  EXPECT_EQ(ProductMessageKey(), GetCurrentMessage());
  FlushEvents();
  EXPECT_TRUE(controller().HasPendingMessagesForTesting());
  EXPECT_EQ(notice.key(), GetCurrentMessage());
  EXPECT_TRUE(notice.has_priority());
  EXPECT_TRUE(notice.received_priority());
  notice.SetShown();
  notice.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
  EXPECT_EQ(ProductMessageKey(), GetCurrentMessage());
  EXPECT_FALSE(notice.has_priority());
  EXPECT_TRUE(notice.received_priority());
  FlushEvents();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
  EXPECT_EQ(ProductMessageKey(), GetCurrentMessage());
}

TEST_F(ProductMessagingControllerTest, QueueMultipleIndependentNotices) {
  test::TestProductMessage notice1(controller(), kNoticeId1);
  test::TestProductMessage notice2(controller(), kNoticeId2);
  test::TestProductMessage notice3(controller(), kNoticeId3);

  // Ensure that only one notice runs at a time, and that once it is marked as
  // done and releases its handle, the next runs.
  std::set<test::TestProductMessage*> remaining{&notice1, &notice2, &notice3};
  while (!remaining.empty()) {
    // Allow the next notice to run.
    FlushEvents();

    // Find the running notice.
    test::TestProductMessage* running = nullptr;
    for (test::TestProductMessage* notice : remaining) {
      if (notice->has_priority()) {
        EXPECT_EQ(nullptr, running);
        running = notice;
        notice->SetShown();
        notice->Release();
        break;
      }
    }
    EXPECT_NE(nullptr, running);
    remaining.erase(running);

    // Ensure that "has pending notices" is reporting properly.
    EXPECT_EQ(!remaining.empty(), controller().HasPendingMessagesForTesting());
  }

  // Ensure all notices have been shown.
  EXPECT_TRUE(remaining.empty());
}

TEST_F(ProductMessagingControllerTest, QueueDependentNotices_NotShown) {
  test::TestProductMessage notice1(controller(), kNoticeId1,
                                   {kNoticeId2, kNoticeId3});
  test::TestProductMessage notice2(controller(), kNoticeId2, {kNoticeId3});
  test::TestProductMessage notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.has_priority());
  notice3.Release();
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
}

TEST_F(ProductMessagingControllerTest, QueueDependentNotices_Shown) {
  test::TestProductMessage notice1(controller(), kNoticeId1,
                                   {kNoticeId2, kNoticeId3});
  test::TestProductMessage notice2(controller(), kNoticeId2, {kNoticeId3});
  test::TestProductMessage notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.has_priority());
  notice3.SetShown();
  notice3.Release();
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.SetShown();
  notice1.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
}

TEST_F(ProductMessagingControllerTest, QueueDependentNoticeChain_NotShown) {
  test::TestProductMessage notice1(controller(), kNoticeId1, {kNoticeId2});
  test::TestProductMessage notice2(controller(), kNoticeId2, {kNoticeId3});
  test::TestProductMessage notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.has_priority());
  notice3.Release();
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
}

TEST_F(ProductMessagingControllerTest, QueueDependentNoticeChain_Shown) {
  test::TestProductMessage notice1(controller(), kNoticeId1, {kNoticeId2});
  test::TestProductMessage notice2(controller(), kNoticeId2, {kNoticeId3});
  test::TestProductMessage notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.has_priority());
  notice3.SetShown();
  notice3.Release();
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.SetShown();
  notice1.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
}

TEST_F(ProductMessagingControllerTest, BlockedBy) {
  test::TestProductMessage notice1(controller(), kNoticeId1, {}, {kNoticeId2});
  test::TestProductMessage notice2(controller(), kNoticeId2);

  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  FlushEvents();
  EXPECT_FALSE(notice1.has_priority());
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
}

TEST_F(ProductMessagingControllerTest, BlockedByNotBlockedIfNotShown) {
  test::TestProductMessage notice1(controller(), kNoticeId1, {}, {kNoticeId2});
  test::TestProductMessage notice2(controller(), kNoticeId2);

  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
}

TEST_F(ProductMessagingControllerTest, BlockedByBlocksLater) {
  test::TestProductMessage notice2(controller(), kNoticeId2);

  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  FlushEvents();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());

  test::TestProductMessage notice1(controller(), kNoticeId1, {}, {kNoticeId2});
  FlushEvents();
  EXPECT_FALSE(notice1.has_priority());
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
}

TEST_F(ProductMessagingControllerTest, BlockedByDoesNotBlockAfterNewSession) {
  test::TestProductMessage notice2(controller(), kNoticeId2);

  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  FlushEvents();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());

  session_provider().StartNewSession();

  test::TestProductMessage notice1(controller(), kNoticeId1, {}, {kNoticeId2});
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.SetShown();
  notice1.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
}

TEST_F(ProductMessagingControllerTest, QueueBlockedByAndDependentNotices) {
  // As soon as notice 2 is purged by notice 3 showing, this notice will be able
  // to show.
  test::TestProductMessage notice1(controller(), kNoticeId1,
                                   {kNoticeId2, kNoticeId3});
  // This will be blocked by the first notice, and not show.
  test::TestProductMessage notice2(controller(), kNoticeId2, {}, {kNoticeId3});
  // This one will show first.
  test::TestProductMessage notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.has_priority());
  notice3.SetShown();
  notice3.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.SetShown();
  notice1.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
}

TEST_F(ProductMessagingControllerTest,
       QueueBlockedByAndDependentNoticesNoticesDoNotShow) {
  test::TestProductMessage notice1(controller(), kNoticeId1, {kNoticeId2},
                                   {kNoticeId3});
  test::TestProductMessage notice2(controller(), kNoticeId2, {}, {kNoticeId3});
  test::TestProductMessage notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.has_priority());
  notice3.Release();
  FlushEvents();
  EXPECT_TRUE(notice2.has_priority());
  notice2.SetShown();
  notice2.Release();
  FlushEvents();
  EXPECT_TRUE(notice1.has_priority());
  notice1.SetShown();
  notice1.Release();
  EXPECT_FALSE(controller().HasPendingMessagesForTesting());
}

TEST_F(ProductMessagingControllerTest, StatusCallbacks) {
  UNCALLED_MOCK_CALLBACK(ProductMessageStatusCallback, status_update);
  const auto sub =
      controller().AddStatusUpdateCallbackForTesting(status_update.Get());

  // Queue one notice.
  EXPECT_CALL(status_update, Run(kNoticeId1, ProductMessageStatus::kQueued));
  test::TestProductMessage notice1(controller(), kNoticeId1);
  EXPECT_CALL(status_update, Run).Times(0);

  // Notice should be granted when events are processed, which should trigger a
  // callback on `granted`.
  EXPECT_CALL_IN_SCOPE(status_update,
                       Run(kNoticeId1, ProductMessageStatus::kEligible),
                       FlushEvents());

  // Queue a second notice.
  EXPECT_CALL(status_update, Run(kNoticeId2, ProductMessageStatus::kQueued));
  test::TestProductMessage notice2(controller(), kNoticeId2);
  EXPECT_CALL(status_update, Run).Times(0);

  // Mark the first notice as shown, triggering the `shown` callback, then
  // complete it.
  EXPECT_CALL_IN_SCOPE(status_update,
                       Run(kNoticeId1, ProductMessageStatus::kShowing),
                       notice1.SetShown());
  notice1.Release();

  // Now the second notice is free to be granted.
  EXPECT_CALL_IN_SCOPE(status_update,
                       Run(kNoticeId2, ProductMessageStatus::kEligible),
                       FlushEvents());

  // End the second notice without showing it; this results in no `shown`
  // callback.
  notice2.Release();
}

}  // namespace user_education
