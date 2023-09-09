// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/product_messaging_controller.h"

#include <initializer_list>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
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
                      std::initializer_list<RequiredNoticeId> show_after = {})
      : id_(id) {
    controller.QueueRequiredNotice(
        id_, base::BindOnce(&TestNotice::OnReadyToShow, base::Unretained(this)),
        show_after);
  }
  ~TestNotice() = default;

  void Done() {
    CHECK(handle_);
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

  ProductMessagingController& controller() { return controller_; }

  void FlushEvents() {
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ProductMessagingController controller_;
};

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

}  // namespace user_education
