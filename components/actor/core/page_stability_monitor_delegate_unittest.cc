// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/actor/core/page_stability_monitor_delegate.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "components/actor/core/page_stability_metrics.h"
#include "components/actor/core/task_id.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/page_content_annotations/core/page_stability_event.h"
#include "components/page_content_annotations/core/page_stability_state.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

namespace {

using page_content_annotations::PageStabilityState;

struct LoggedEvent {
  TaskId task_id;
  mojom::JournalEntryType type;
  std::string event_name;
};

class FakePageStabilityMetrics : public PageStabilityMetrics {
 public:
  FakePageStabilityMetrics() = default;
  FakePageStabilityMetrics(const FakePageStabilityMetrics&) = delete;
  FakePageStabilityMetrics& operator=(const FakePageStabilityMetrics&) = delete;
  ~FakePageStabilityMetrics() override = default;

  void Start() override { start_called = true; }
  void WillMoveToState(PageStabilityState state) override {
    last_state = state;
  }
  void OnNetworkAndMainThreadIdle() override { idle_called = true; }
  void OnPaintStabilityReached() override { paint_stability_called = true; }
  void OnInteractionContentfulPaint() override {
    interaction_paint_called = true;
  }
  void Flush() override { flush_called = true; }

  bool start_called = false;
  bool idle_called = false;
  bool paint_stability_called = false;
  bool interaction_paint_called = false;
  bool flush_called = false;
  std::optional<PageStabilityState> last_state;
};

class FakePageStabilityMonitorDelegate : public PageStabilityMonitorDelegate {
 public:
  FakePageStabilityMonitorDelegate(
      TaskId task_id,
      base::RepeatingCallback<void(TaskId,
                                   mojom::JournalEntryType,
                                   std::string_view)> log_event_callback,
      std::unique_ptr<PageStabilityMetrics> metrics = nullptr)
      : PageStabilityMonitorDelegate(task_id, {}),
        log_event_callback_(std::move(log_event_callback)),
        metrics_(std::move(metrics)) {}

  FakePageStabilityMonitorDelegate(const FakePageStabilityMonitorDelegate&) =
      delete;
  FakePageStabilityMonitorDelegate& operator=(
      const FakePageStabilityMonitorDelegate&) = delete;
  ~FakePageStabilityMonitorDelegate() override = default;

 protected:
  void LogEvent(mojom::JournalEntryType type,
                std::string_view event_name,
                std::vector<mojom::JournalDetailsPtr> details) override {
    log_event_callback_.Run(task_id(), type, event_name);
  }

  std::unique_ptr<PageStabilityMetrics> CreateMetrics() override {
    return metrics_ ? std::move(metrics_)
                    : PageStabilityMonitorDelegate::CreateMetrics();
  }

 private:
  base::RepeatingCallback<
      void(TaskId, mojom::JournalEntryType, std::string_view)>
      log_event_callback_;
  std::unique_ptr<PageStabilityMetrics> metrics_;
};

class PageStabilityMonitorDelegateTest : public testing::Test {
 public:
  PageStabilityMonitorDelegateTest() = default;

  void LogEvent(TaskId task_id,
                mojom::JournalEntryType type,
                std::string_view event_name) {
    logged_events_.push_back({task_id, type, std::string(event_name)});
  }

  FakePageStabilityMonitorDelegate CreateDelegate(
      TaskId task_id,
      std::unique_ptr<PageStabilityMetrics> metrics = nullptr) {
    return FakePageStabilityMonitorDelegate(
        task_id,
        base::BindRepeating(&PageStabilityMonitorDelegateTest::LogEvent,
                            base::Unretained(this)),
        std::move(metrics));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::vector<LoggedEvent> logged_events_;
};

// Test that moving between states logs begin and end journal events.
TEST_F(PageStabilityMonitorDelegateTest, WillMoveToStateLogsBeginAndEnd) {
  TaskId task_id = TaskId::FromUnsafeValue(1);
  FakePageStabilityMonitorDelegate delegate = CreateDelegate(task_id);

  delegate.WillMoveToState(PageStabilityState::kStartMonitoring);
  delegate.WillMoveToState(PageStabilityState::kTimeout);

  ASSERT_EQ(logged_events_.size(), 3u);
  EXPECT_EQ(logged_events_[0].task_id, task_id);
  EXPECT_EQ(logged_events_[0].type, mojom::JournalEntryType::kBegin);
  EXPECT_EQ(logged_events_[0].event_name,
            "PageStabilityState: StartMonitoring");
  EXPECT_EQ(logged_events_[1].type, mojom::JournalEntryType::kEnd);
  EXPECT_EQ(logged_events_[1].event_name,
            "PageStabilityState: StartMonitoring");
  EXPECT_EQ(logged_events_[2].type, mojom::JournalEntryType::kBegin);
  EXPECT_EQ(logged_events_[2].event_name, "PageStabilityState: Timeout");
}

// Test that teardown events close any active journal entry.
TEST_F(PageStabilityMonitorDelegateTest, TearDownEventEndsActiveEntry) {
  TaskId task_id = TaskId::FromUnsafeValue(1);
  FakePageStabilityMonitorDelegate delegate = CreateDelegate(task_id);

  delegate.WillMoveToState(PageStabilityState::kStartMonitoring);
  delegate.OnEvent(
      page_content_annotations::PageStabilityMonitorTearDownEvent{});

  ASSERT_EQ(logged_events_.size(), 2u);
  EXPECT_EQ(logged_events_[0].type, mojom::JournalEntryType::kBegin);
  EXPECT_EQ(logged_events_[1].type, mojom::JournalEntryType::kEnd);
  EXPECT_EQ(logged_events_[1].event_name,
            "PageStabilityState: StartMonitoring");
}

// Test that default CreateMetrics returns nullptr and events are handled
// safely.
TEST_F(PageStabilityMonitorDelegateTest,
       DefaultMetricsReturnsNullAndHandlesEventsSafely) {
  TaskId task_id = TaskId::FromUnsafeValue(1);
  FakePageStabilityMonitorDelegate delegate = CreateDelegate(task_id);

  delegate.OnEvent(page_content_annotations::PageStabilityMonitorStartEvent{});
  delegate.OnEvent(page_content_annotations::InteractionContentfulPaintEvent{});
  delegate.OnEvent(page_content_annotations::NetworkAndMainThreadIdleEvent{});
  delegate.OnEvent(page_content_annotations::PaintStabilityReachedEvent{});
  delegate.WillMoveToState(PageStabilityState::kStartMonitoring);
  delegate.WillMoveToState(PageStabilityState::kTimeout);
  delegate.WillMoveToState(PageStabilityState::kDone);
  delegate.OnEvent(page_content_annotations::PageStabilityMonitorStopEvent{});
}

// Test that when a custom metrics recorder is supplied, the delegate dispatches
// all lifecycle and paint events to it.
TEST_F(PageStabilityMonitorDelegateTest,
       CustomMetricsReceivesDispatchedEvents) {
  auto fake_metrics = std::make_unique<FakePageStabilityMetrics>();
  FakePageStabilityMetrics* raw_metrics = fake_metrics.get();

  TaskId task_id = TaskId::FromUnsafeValue(1);
  FakePageStabilityMonitorDelegate delegate =
      CreateDelegate(task_id, std::move(fake_metrics));

  delegate.OnEvent(page_content_annotations::PageStabilityMonitorStartEvent{});
  EXPECT_TRUE(raw_metrics->start_called);

  delegate.OnEvent(page_content_annotations::InteractionContentfulPaintEvent{});
  EXPECT_TRUE(raw_metrics->interaction_paint_called);

  delegate.OnEvent(page_content_annotations::NetworkAndMainThreadIdleEvent{});
  EXPECT_TRUE(raw_metrics->idle_called);

  delegate.OnEvent(page_content_annotations::PaintStabilityReachedEvent{});
  EXPECT_TRUE(raw_metrics->paint_stability_called);

  delegate.WillMoveToState(PageStabilityState::kStartMonitoring);
  EXPECT_EQ(raw_metrics->last_state, PageStabilityState::kStartMonitoring);

  delegate.OnEvent(page_content_annotations::PageStabilityMonitorStopEvent{});
  EXPECT_TRUE(raw_metrics->flush_called);
}

}  // namespace

}  // namespace actor
