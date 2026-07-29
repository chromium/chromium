// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/page_stability_monitor_delegate.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/page_content_annotations/core/page_stability_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

struct LoggedEvent {
  TaskId task_id;
  mojom::JournalEntryType type;
  std::string event_name;
};

class FakePageStabilityMonitorDelegate : public PageStabilityMonitorDelegate {
 public:
  FakePageStabilityMonitorDelegate(
      TaskId task_id,
      base::RepeatingCallback<void(TaskId,
                                   mojom::JournalEntryType,
                                   std::string_view)> log_event_callback,
      Thresholds thresholds)
      : PageStabilityMonitorDelegate(task_id, thresholds),
        log_event_callback_(std::move(log_event_callback)) {}

  ~FakePageStabilityMonitorDelegate() override = default;

 protected:
  void LogEvent(mojom::JournalEntryType type,
                std::string_view event_name,
                std::vector<mojom::JournalDetailsPtr> details) override {
    log_event_callback_.Run(task_id(), type, event_name);
  }

 private:
  base::RepeatingCallback<
      void(TaskId, mojom::JournalEntryType, std::string_view)>
      log_event_callback_;
};

class PageStabilityMonitorDelegateTest : public testing::Test {
 public:
  PageStabilityMonitorDelegateTest() = default;

  void LogEvent(TaskId task_id,
                mojom::JournalEntryType type,
                std::string_view event_name) {
    logged_events_.push_back({task_id, type, std::string(event_name)});
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::vector<LoggedEvent> logged_events_;
};

TEST_F(PageStabilityMonitorDelegateTest, WillMoveToStateLogsBeginAndEnd) {
  TaskId task_id = TaskId::FromUnsafeValue(1);
  FakePageStabilityMonitorDelegate delegate(
      task_id,
      base::BindRepeating(&PageStabilityMonitorDelegateTest::LogEvent,
                          base::Unretained(this)),
      PageStabilityMonitorDelegate::Thresholds{
          .timeout_delay = base::Seconds(1),
          .min_wait = base::Seconds(1),
          .initial_paint_timeout = base::Seconds(1),
          .subsequent_paint_timeout = base::Seconds(1),
      });

  delegate.WillMoveToState(
      page_content_annotations::PageStabilityState::kStartMonitoring);

  ASSERT_EQ(logged_events_.size(), 1u);
  EXPECT_EQ(logged_events_[0].task_id, task_id);
  EXPECT_EQ(logged_events_[0].type, mojom::JournalEntryType::kBegin);
  EXPECT_EQ(logged_events_[0].event_name,
            "PageStabilityState: StartMonitoring");

  // Move to another state.
  delegate.WillMoveToState(
      page_content_annotations::PageStabilityState::kTimeout);

  ASSERT_EQ(logged_events_.size(), 3u);
  EXPECT_EQ(logged_events_[1].type, mojom::JournalEntryType::kEnd);
  EXPECT_EQ(logged_events_[1].event_name,
            "PageStabilityState: StartMonitoring");
  EXPECT_EQ(logged_events_[2].type, mojom::JournalEntryType::kBegin);
  EXPECT_EQ(logged_events_[2].event_name, "PageStabilityState: Timeout");
}

TEST_F(PageStabilityMonitorDelegateTest, TearDownEventEndsActiveEntry) {
  TaskId task_id = TaskId::FromUnsafeValue(1);
  FakePageStabilityMonitorDelegate delegate(
      task_id,
      base::BindRepeating(&PageStabilityMonitorDelegateTest::LogEvent,
                          base::Unretained(this)),
      PageStabilityMonitorDelegate::Thresholds{
          .timeout_delay = base::Seconds(1),
          .min_wait = base::Seconds(1),
          .initial_paint_timeout = base::Seconds(1),
          .subsequent_paint_timeout = base::Seconds(1),
      });

  delegate.WillMoveToState(
      page_content_annotations::PageStabilityState::kStartMonitoring);
  ASSERT_EQ(logged_events_.size(), 1u);
  EXPECT_EQ(logged_events_[0].type, mojom::JournalEntryType::kBegin);

  // Dispatch the TearDown event.
  delegate.OnEvent(
      page_content_annotations::PageStabilityMonitorTearDownEvent{});

  // After teardown, the active entry must be closed.
  ASSERT_EQ(logged_events_.size(), 2u);
  EXPECT_EQ(logged_events_[1].type, mojom::JournalEntryType::kEnd);
  EXPECT_EQ(logged_events_[1].event_name,
            "PageStabilityState: StartMonitoring");
}

}  // namespace

}  // namespace actor
