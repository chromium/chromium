// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_insights/event_queue.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/private_insights/events/contextual_cue_log_event.pb.h"
#include "components/private_insights/events/storage_access_log_event.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_insights {

namespace {

constexpr char kPrefix[] = "PrivateMetrics.PrivateInsights.TestEvents";
constexpr char kQueuedCountHistogram[] =
    "PrivateMetrics.PrivateInsights.TestEvents.Logging.QueuedCount";
constexpr char kRemovedCountHistogram[] =
    "PrivateMetrics.PrivateInsights.TestEvents.Logging.RemovedCount";
constexpr char kRequeuedCountHistogram[] =
    "PrivateMetrics.PrivateInsights.TestEvents.Requeuing.RequeuedCount";
constexpr char kDroppedCountHistogram[] =
    "PrivateMetrics.PrivateInsights.TestEvents.Requeuing.DroppedCount";

using ContextualCueEventQueue = EventQueue<events::ContextualCueLogEvent>;

events::ContextualCueLogEvent MakeCueEvent(const std::string& cue_id) {
  events::ContextualCueLogEvent event;
  event.set_cue_id(cue_id);
  return event;
}

// Returns the cue ids of `entries`, oldest first.
std::vector<std::string> CueIds(
    const ContextualCueEventQueue::Entries& entries) {
  std::vector<std::string> ids;
  for (const auto& entry : entries) {
    ids.push_back(entry.event.cue_id());
  }
  return ids;
}

class EventQueueTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  ContextualCueEventQueue queue_{kPrefix};
};

TEST_F(EventQueueTest, StartsEmpty) {
  EXPECT_TRUE(queue_.empty());
  EXPECT_EQ(queue_.size(), 0u);
  EXPECT_TRUE(queue_.entries().empty());
}

TEST_F(EventQueueTest, PushAppendsAndStampsTime) {
  const base::Time time1 = base::Time::Now();
  queue_.Push(MakeCueEvent("cue_1"), /*max_events=*/10);

  task_environment_.FastForwardBy(base::Minutes(5));
  const base::Time time2 = base::Time::Now();
  queue_.Push(MakeCueEvent("cue_2"), /*max_events=*/10);

  ASSERT_EQ(queue_.size(), 2u);
  EXPECT_EQ(CueIds(queue_.entries()),
            (std::vector<std::string>{"cue_1", "cue_2"}));
  EXPECT_EQ(queue_.entries()[0].timestamp, time1);
  EXPECT_EQ(queue_.entries()[1].timestamp, time2);

  histogram_tester_.ExpectBucketCount(kQueuedCountHistogram, 1, 1);
  histogram_tester_.ExpectBucketCount(kQueuedCountHistogram, 2, 1);
  histogram_tester_.ExpectTotalCount(kRemovedCountHistogram, 0);
}

TEST_F(EventQueueTest, PushEvictsOldestBeyondMaxEvents) {
  queue_.Push(MakeCueEvent("cue_1"), /*max_events=*/2);
  queue_.Push(MakeCueEvent("cue_2"), /*max_events=*/2);
  queue_.Push(MakeCueEvent("cue_3"), /*max_events=*/2);

  EXPECT_EQ(CueIds(queue_.entries()),
            (std::vector<std::string>{"cue_2", "cue_3"}));
  histogram_tester_.ExpectBucketCount(kQueuedCountHistogram, 2, 2);
  histogram_tester_.ExpectUniqueSample(kRemovedCountHistogram, 1, 1);
}

TEST_F(EventQueueTest, PushEvictsMultipleWhenMaxEventsShrinks) {
  queue_.Push(MakeCueEvent("cue_1"), /*max_events=*/10);
  queue_.Push(MakeCueEvent("cue_2"), /*max_events=*/10);
  queue_.Push(MakeCueEvent("cue_3"), /*max_events=*/10);

  // A smaller cap on a later push (e.g. a Finch param change) evicts as many
  // of the oldest events as needed.
  queue_.Push(MakeCueEvent("cue_4"), /*max_events=*/2);

  EXPECT_EQ(CueIds(queue_.entries()),
            (std::vector<std::string>{"cue_3", "cue_4"}));
  histogram_tester_.ExpectUniqueSample(kRemovedCountHistogram, 2, 1);
}

// `max_events` comes from a Finch param, which could be misconfigured to zero
// or a negative value. Both must keep nothing rather than crash.
TEST_F(EventQueueTest, PushWithNonPositiveMaxEventsKeepsNothing) {
  queue_.Push(MakeCueEvent("cue_1"), /*max_events=*/0);
  EXPECT_TRUE(queue_.empty());
  histogram_tester_.ExpectBucketCount(kRemovedCountHistogram, 1, 1);
  histogram_tester_.ExpectBucketCount(kQueuedCountHistogram, 0, 1);

  queue_.Push(MakeCueEvent("cue_2"), /*max_events=*/-1);
  EXPECT_TRUE(queue_.empty());
  histogram_tester_.ExpectBucketCount(kRemovedCountHistogram, 1, 2);
  histogram_tester_.ExpectBucketCount(kQueuedCountHistogram, 0, 2);

  // A negative cap on a non-empty queue evicts everything that was there.
  queue_.Push(MakeCueEvent("cue_3"), /*max_events=*/10);
  queue_.Push(MakeCueEvent("cue_4"), /*max_events=*/10);
  queue_.Push(MakeCueEvent("cue_5"), /*max_events=*/-5);
  EXPECT_TRUE(queue_.empty());
  histogram_tester_.ExpectBucketCount(kRemovedCountHistogram, 3, 1);
}

TEST_F(EventQueueTest, RequeueWithNonPositiveMaxEventsDropsEverything) {
  queue_.Push(MakeCueEvent("cue_1"), /*max_events=*/10);
  queue_.Push(MakeCueEvent("cue_2"), /*max_events=*/10);
  ContextualCueEventQueue::Entries in_flight = queue_.TakeAll();

  queue_.Requeue(std::move(in_flight), /*max_events=*/-1);
  EXPECT_TRUE(queue_.empty());
  histogram_tester_.ExpectUniqueSample(kRequeuedCountHistogram, 0, 1);
  histogram_tester_.ExpectUniqueSample(kDroppedCountHistogram, 2, 1);
}

TEST_F(EventQueueTest, TakeAllDetachesAndLeavesQueueReusable) {
  queue_.Push(MakeCueEvent("cue_1"), /*max_events=*/10);
  queue_.Push(MakeCueEvent("cue_2"), /*max_events=*/10);

  ContextualCueEventQueue::Entries taken = queue_.TakeAll();
  EXPECT_EQ(CueIds(taken), (std::vector<std::string>{"cue_1", "cue_2"}));
  EXPECT_TRUE(queue_.empty());

  // The queue keeps working after being drained.
  queue_.Push(MakeCueEvent("cue_3"), /*max_events=*/10);
  EXPECT_EQ(CueIds(queue_.entries()), (std::vector<std::string>{"cue_3"}));
  EXPECT_EQ(CueIds(queue_.TakeAll()), (std::vector<std::string>{"cue_3"}));

  // TakeAll() on an empty queue returns an empty batch.
  EXPECT_TRUE(queue_.empty());
  EXPECT_TRUE(queue_.TakeAll().empty());
}

TEST_F(EventQueueTest, TakeAllDoesNotRecordHistograms) {
  queue_.Push(MakeCueEvent("cue_1"), /*max_events=*/10);
  histogram_tester_.ExpectTotalCount(kQueuedCountHistogram, 1);

  queue_.TakeAll();
  histogram_tester_.ExpectTotalCount(kQueuedCountHistogram, 1);
  histogram_tester_.ExpectTotalCount(kRemovedCountHistogram, 0);
  histogram_tester_.ExpectTotalCount(kRequeuedCountHistogram, 0);
  histogram_tester_.ExpectTotalCount(kDroppedCountHistogram, 0);
}

TEST_F(EventQueueTest, RequeueEmptyIsNoOp) {
  queue_.Push(MakeCueEvent("cue_1"), /*max_events=*/10);

  queue_.Requeue(ContextualCueEventQueue::Entries(), /*max_events=*/10);

  EXPECT_EQ(CueIds(queue_.entries()), (std::vector<std::string>{"cue_1"}));
  histogram_tester_.ExpectTotalCount(kRequeuedCountHistogram, 0);
  histogram_tester_.ExpectTotalCount(kDroppedCountHistogram, 0);
}

TEST_F(EventQueueTest, RequeuePrependsPreservingOrder) {
  queue_.Push(MakeCueEvent("cue_1"), /*max_events=*/10);
  queue_.Push(MakeCueEvent("cue_2"), /*max_events=*/10);
  ContextualCueEventQueue::Entries in_flight = queue_.TakeAll();

  // New events arrive while the batch is in flight.
  queue_.Push(MakeCueEvent("cue_3"), /*max_events=*/10);
  queue_.Push(MakeCueEvent("cue_4"), /*max_events=*/10);

  queue_.Requeue(std::move(in_flight), /*max_events=*/10);

  EXPECT_EQ(CueIds(queue_.entries()),
            (std::vector<std::string>{"cue_1", "cue_2", "cue_3", "cue_4"}));
  histogram_tester_.ExpectUniqueSample(kRequeuedCountHistogram, 2, 1);
  histogram_tester_.ExpectUniqueSample(kDroppedCountHistogram, 0, 1);
}

TEST_F(EventQueueTest, RequeueDropsOldestWhenExceedingMaxEvents) {
  queue_.Push(MakeCueEvent("cue_1"), /*max_events=*/2);
  queue_.Push(MakeCueEvent("cue_2"), /*max_events=*/2);
  ContextualCueEventQueue::Entries in_flight = queue_.TakeAll();

  queue_.Push(MakeCueEvent("cue_3"), /*max_events=*/2);

  queue_.Requeue(std::move(in_flight), /*max_events=*/2);

  // Only one slot is free; the newest in-flight event ("cue_2") takes it and
  // the oldest ("cue_1") is dropped.
  EXPECT_EQ(CueIds(queue_.entries()),
            (std::vector<std::string>{"cue_2", "cue_3"}));
  histogram_tester_.ExpectUniqueSample(kRequeuedCountHistogram, 1, 1);
  histogram_tester_.ExpectUniqueSample(kDroppedCountHistogram, 1, 1);
}

TEST_F(EventQueueTest, RequeueIntoFullQueueDropsEverything) {
  queue_.Push(MakeCueEvent("cue_1"), /*max_events=*/1);
  ContextualCueEventQueue::Entries in_flight = queue_.TakeAll();

  queue_.Push(MakeCueEvent("cue_2"), /*max_events=*/1);

  queue_.Requeue(std::move(in_flight), /*max_events=*/1);

  EXPECT_EQ(CueIds(queue_.entries()), (std::vector<std::string>{"cue_2"}));
  histogram_tester_.ExpectUniqueSample(kRequeuedCountHistogram, 0, 1);
  histogram_tester_.ExpectUniqueSample(kDroppedCountHistogram, 1, 1);
}

TEST_F(EventQueueTest, WeakPtrInvalidatedOnDestruction) {
  auto queue = std::make_unique<ContextualCueEventQueue>(kPrefix);
  base::WeakPtr<ContextualCueEventQueue> weak = queue->GetWeakPtr();
  ASSERT_TRUE(weak);

  queue->Push(MakeCueEvent("cue_1"), /*max_events=*/10);
  ContextualCueEventQueue::Entries in_flight = queue->TakeAll();

  // Mirrors the upload pipeline: the owner goes away while a batch is in
  // flight, so the requeue callback must observe an invalid WeakPtr.
  queue.reset();
  EXPECT_FALSE(weak);
}

// Two populations with different event protos each own an independent queue
// with independent histograms.
TEST_F(EventQueueTest, QueuesForDifferentProtosAreIndependent) {
  EventQueue<events::StorageAccessLogEvent> storage_access_queue(
      "PrivateMetrics.PrivateInsights.StorageAccessEvents");

  events::StorageAccessLogEvent storage_access_event;
  storage_access_event.set_domain("example.com");
  storage_access_queue.Push(std::move(storage_access_event), /*max_events=*/1);
  storage_access_queue.Push(events::StorageAccessLogEvent(), /*max_events=*/1);

  queue_.Push(MakeCueEvent("cue_1"), /*max_events=*/10);

  EXPECT_EQ(queue_.size(), 1u);
  EXPECT_EQ(storage_access_queue.size(), 1u);

  histogram_tester_.ExpectUniqueSample(kQueuedCountHistogram, 1, 1);
  histogram_tester_.ExpectTotalCount(kRemovedCountHistogram, 0);
  histogram_tester_.ExpectBucketCount(
      "PrivateMetrics.PrivateInsights.StorageAccessEvents.Logging.QueuedCount",
      1, 2);
  histogram_tester_.ExpectUniqueSample(
      "PrivateMetrics.PrivateInsights.StorageAccessEvents.Logging."
      "RemovedCount",
      1, 1);
}

}  // namespace

}  // namespace private_insights
