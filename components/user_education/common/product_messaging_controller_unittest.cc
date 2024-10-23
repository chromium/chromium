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
#include "components/user_education/test/test_user_education_storage_service.h"
#include "components/user_education/test/user_education_session_mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_education {

namespace {

DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(kNoticeId1);
DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(kNoticeId2);
DEFINE_LOCAL_REQUIRED_NOTICE_IDENTIFIER(kNoticeId3);

class TestNotice {
 public:
  explicit TestNotice(ProductMessagingController& controller,
                      RequiredNoticeId id,
                      std::initializer_list<RequiredNoticeId> show_after = {},
                      std::initializer_list<RequiredNoticeId> blocked_by = {})
      : id_(id) {
    controller.QueueRequiredNotice(
        id_, base::BindOnce(&TestNotice::OnReadyToShow, base::Unretained(this)),
        show_after, blocked_by);
  }
  ~TestNotice() = default;

  void Done(bool shown = true) {
    CHECK(handle_);
    if (shown) {
      handle_.SetShown();
    }
    handle_.Release();
  }

  RequiredNoticeId id() const { return id_; }
  bool shown() const { return shown_; }
  bool showing() const { return static_cast<bool>(handle_); }

 private:
  void OnReadyToShow(RequiredNoticePriorityHandle handle) {
    shown_ = true;
    handle_ = std::move(handle);
  }

  const RequiredNoticeId id_;
  bool shown_ = false;
  RequiredNoticePriorityHandle handle_;
};

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

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::TestUserEducationSessionProvider session_provider_{false};
  test::TestUserEducationStorageService storage_service_;
  ProductMessagingController controller_;
};

TEST_F(ProductMessagingControllerTest, ConditionallyRecordsDone) {
  TestNotice notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.showing());
  notice.Done();
  EXPECT_FALSE(controller().has_pending_notices());
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::UnorderedElementsAre(kNoticeId1.GetName()));

  TestNotice notice2(controller(), kNoticeId2);
  FlushEvents();
  notice2.Done(false);
  EXPECT_FALSE(controller().has_pending_notices());
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::UnorderedElementsAre(kNoticeId1.GetName()));

  TestNotice notice3(controller(), kNoticeId3);
  FlushEvents();
  notice3.Done(true);
  EXPECT_FALSE(controller().has_pending_notices());
  EXPECT_THAT(storage_service().ReadProductMessagingData().shown_notices,
              testing::UnorderedElementsAre(kNoticeId1.GetName(),
                                            kNoticeId3.GetName()));
}

TEST_F(ProductMessagingControllerTest, ShownBlocksSelf) {
  TestNotice notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.showing());
  notice.Done();
  EXPECT_FALSE(notice.showing());

  TestNotice notice2(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_FALSE(notice2.showing());
}

TEST_F(ProductMessagingControllerTest, NotShownDoesNotBlockSelf) {
  TestNotice notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.showing());
  notice.Done(false);
  EXPECT_FALSE(notice.showing());

  TestNotice notice2(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice2.showing());
  notice2.Done();
  EXPECT_FALSE(notice2.showing());
}

TEST_F(ProductMessagingControllerTest, ClearsOnNewSession) {
  TestNotice notice(controller(), kNoticeId1);
  FlushEvents();
  EXPECT_TRUE(notice.showing());
  notice.Done();
  EXPECT_FALSE(controller().has_pending_notices());
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
  EXPECT_FALSE(controller().has_pending_notices());
  EXPECT_EQ(RequiredNoticeId(), controller().current_notice_for_testing());
  TestNotice notice(controller(), kNoticeId1);
  EXPECT_TRUE(controller().has_pending_notices());
  EXPECT_EQ(RequiredNoticeId(), controller().current_notice_for_testing());
  FlushEvents();
  EXPECT_TRUE(controller().has_pending_notices());
  EXPECT_EQ(notice.id(), controller().current_notice_for_testing());
  EXPECT_TRUE(notice.showing());
  EXPECT_TRUE(notice.shown());
  notice.Done();
  EXPECT_FALSE(controller().has_pending_notices());
  EXPECT_EQ(RequiredNoticeId(), controller().current_notice_for_testing());
  EXPECT_FALSE(notice.showing());
  EXPECT_TRUE(notice.shown());
  FlushEvents();
  EXPECT_FALSE(controller().has_pending_notices());
  EXPECT_EQ(RequiredNoticeId(), controller().current_notice_for_testing());
}

TEST_F(ProductMessagingControllerTest, QueueMultipleIndependentNotices) {
  TestNotice notice1(controller(), kNoticeId1);
  TestNotice notice2(controller(), kNoticeId2);
  TestNotice notice3(controller(), kNoticeId3);

  // Ensure that only one notice runs at a time, and that once it is marked as
  // done and releases its handle, the next runs.
  std::set<TestNotice*> remaining{&notice1, &notice2, &notice3};
  while (!remaining.empty()) {
    // Allow the next notice to run.
    FlushEvents();

    // Find the running notice.
    TestNotice* running = nullptr;
    for (TestNotice* notice : remaining) {
      if (notice->showing()) {
        EXPECT_EQ(nullptr, running);
        running = notice;
        notice->Done();
        break;
      }
    }
    EXPECT_NE(nullptr, running);
    remaining.erase(running);

    // Ensure that "has pending notices" is reporting properly.
    EXPECT_EQ(!remaining.empty(), controller().has_pending_notices());
  }

  // Ensure all notices have been shown.
  EXPECT_TRUE(remaining.empty());
}

TEST_F(ProductMessagingControllerTest, QueueDependentNotices) {
  TestNotice notice1(controller(), kNoticeId1, {kNoticeId2, kNoticeId3});
  TestNotice notice2(controller(), kNoticeId2, {kNoticeId3});
  TestNotice notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.showing());
  notice3.Done();
  FlushEvents();
  EXPECT_TRUE(notice2.showing());
  notice2.Done();
  FlushEvents();
  EXPECT_TRUE(notice1.showing());
  notice1.Done();
  EXPECT_FALSE(controller().has_pending_notices());
}

TEST_F(ProductMessagingControllerTest, QueueDependentNoticeChain) {
  TestNotice notice1(controller(), kNoticeId1, {kNoticeId2});
  TestNotice notice2(controller(), kNoticeId2, {kNoticeId3});
  TestNotice notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.showing());
  notice3.Done();
  FlushEvents();
  EXPECT_TRUE(notice2.showing());
  notice2.Done();
  FlushEvents();
  EXPECT_TRUE(notice1.showing());
  notice1.Done();
  EXPECT_FALSE(controller().has_pending_notices());
}

TEST_F(ProductMessagingControllerTest, BlockedBy) {
  TestNotice notice1(controller(), kNoticeId1, {}, {kNoticeId2});
  TestNotice notice2(controller(), kNoticeId2);

  FlushEvents();
  EXPECT_TRUE(notice2.showing());
  notice2.Done();
  FlushEvents();
  EXPECT_FALSE(notice1.showing());
  EXPECT_FALSE(controller().has_pending_notices());
}

TEST_F(ProductMessagingControllerTest, BlockedByNotBlockedIfNotShown) {
  TestNotice notice1(controller(), kNoticeId1, {}, {kNoticeId2});
  TestNotice notice2(controller(), kNoticeId2);

  FlushEvents();
  EXPECT_TRUE(notice2.showing());
  notice2.Done(false);
  FlushEvents();
  EXPECT_TRUE(notice1.showing());
  notice1.Done();
  EXPECT_FALSE(controller().has_pending_notices());
}

TEST_F(ProductMessagingControllerTest, BlockedByBlocksLater) {
  TestNotice notice2(controller(), kNoticeId2);

  FlushEvents();
  EXPECT_TRUE(notice2.showing());
  notice2.Done();
  FlushEvents();
  EXPECT_FALSE(controller().has_pending_notices());

  TestNotice notice1(controller(), kNoticeId1, {}, {kNoticeId2});
  FlushEvents();
  EXPECT_FALSE(notice1.showing());
  EXPECT_FALSE(controller().has_pending_notices());
}

TEST_F(ProductMessagingControllerTest, BlockedByDoesNotBlockAfterNewSession) {
  TestNotice notice2(controller(), kNoticeId2);

  FlushEvents();
  EXPECT_TRUE(notice2.showing());
  notice2.Done();
  FlushEvents();
  EXPECT_FALSE(controller().has_pending_notices());

  session_provider().StartNewSession();

  TestNotice notice1(controller(), kNoticeId1, {}, {kNoticeId2});
  FlushEvents();
  EXPECT_TRUE(notice1.showing());
  notice1.Done();
  EXPECT_FALSE(controller().has_pending_notices());
}

TEST_F(ProductMessagingControllerTest, QueueBlockedByAndDependentNotices) {
  // As soon as notice 2 is purged by notice 3 showing, this notice will be able
  // to show.
  TestNotice notice1(controller(), kNoticeId1, {kNoticeId2, kNoticeId3});
  // This will be blocked by the first notice, and not show.
  TestNotice notice2(controller(), kNoticeId2, {}, {kNoticeId3});
  // This one will show first.
  TestNotice notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.showing());
  notice3.Done();
  FlushEvents();
  EXPECT_TRUE(notice1.showing());
  notice1.Done();
  EXPECT_FALSE(controller().has_pending_notices());
}

TEST_F(ProductMessagingControllerTest,
       QueueBlockedByAndDependentNoticesNoticesDoNotShow) {
  TestNotice notice1(controller(), kNoticeId1, {kNoticeId2}, {kNoticeId3});
  TestNotice notice2(controller(), kNoticeId2, {}, {kNoticeId3});
  TestNotice notice3(controller(), kNoticeId3);

  FlushEvents();
  EXPECT_TRUE(notice3.showing());
  notice3.Done(false);
  FlushEvents();
  EXPECT_TRUE(notice2.showing());
  notice2.Done();
  FlushEvents();
  EXPECT_TRUE(notice1.showing());
  notice1.Done();
  EXPECT_FALSE(controller().has_pending_notices());
}

}  // namespace user_education
