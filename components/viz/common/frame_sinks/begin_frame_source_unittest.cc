// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/begin_frame_source.h"

#include <stdint.h>

#include <memory>

#include "base/test/test_mock_time_task_runner.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/begin_frame_source_test.h"
#include "components/viz/test/fake_delay_based_time_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::_;

namespace viz {
namespace {

// Returns a fake TimeTicks based on the given microsecond offset.
base::TimeTicks TicksFromMicroseconds(int64_t micros) {
  return base::TimeTicks() + base::TimeDelta::FromMicroseconds(micros);
}

// BeginFrameSource testing ----------------------------------------------------
TEST(BeginFrameSourceTest, SourceIdsAreUnique) {
  StubBeginFrameSource source1;
  StubBeginFrameSource source2;
  StubBeginFrameSource source3;
  EXPECT_NE(source1.source_id(), source2.source_id());
  EXPECT_NE(source1.source_id(), source3.source_id());
  EXPECT_NE(source2.source_id(), source3.source_id());
}

class TestTaskRunner : public base::TestMockTimeTaskRunner {
 public:
  TestTaskRunner()
      : base::TestMockTimeTaskRunner(
            base::TestMockTimeTaskRunner::Type::kStandalone) {
    AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(1000));
  }

  void FastForwardTo(base::TimeTicks end_time) {
    base::TimeDelta offset = end_time - NowTicks();
    DCHECK_GE(offset, base::TimeDelta());
    FastForwardBy(offset);
  }

 private:
  ~TestTaskRunner() override = default;  // Ref-counted.
  DISALLOW_COPY_AND_ASSIGN(TestTaskRunner);
};

// BackToBackBeginFrameSource testing
// ------------------------------------------
class BackToBackBeginFrameSourceTest : public ::testing::Test {
 protected:
  static const int64_t kDeadline;
  static const int64_t kInterval;

  void SetUp() override {
    task_runner_ = base::MakeRefCounted<TestTaskRunner>();
    std::unique_ptr<FakeDelayBasedTimeSource> time_source =
        std::make_unique<FakeDelayBasedTimeSource>(
            task_runner_->GetMockTickClock(), task_runner_.get());

    delay_based_time_source_ = time_source.get();
    source_.reset(new BackToBackBeginFrameSource(std::move(time_source)));
    obs_ = std::make_unique<::testing::NiceMock<MockBeginFrameObserver>>();
  }

  void TearDown() override { obs_.reset(); }

  scoped_refptr<TestTaskRunner> task_runner_;
  std::unique_ptr<BackToBackBeginFrameSource> source_;
  std::unique_ptr<MockBeginFrameObserver> obs_;
  FakeDelayBasedTimeSource* delay_based_time_source_;  // Owned by |source_|.
};

const int64_t BackToBackBeginFrameSourceTest::kDeadline =
    BeginFrameArgs::DefaultInterval().InMicroseconds();

const int64_t BackToBackBeginFrameSourceTest::kInterval =
    BeginFrameArgs::DefaultInterval().InMicroseconds();

TEST_F(BackToBackBeginFrameSourceTest, AddObserverSendsBeginFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_TRUE(task_runner_->HasPendingTask());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 1100,
                          1100 + kDeadline, kInterval);
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get());
  task_runner_->RunUntilIdle();
}

TEST_F(BackToBackBeginFrameSourceTest,
       RemoveObserverThenDidFinishFrameProducesNoFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  source_->RemoveObserver(obs_.get());
  source_->DidFinishFrame(obs_.get());

  // Verify no BeginFrame is sent to |obs_|. There is a pending task in the
  // task_runner_ as a BeginFrame was posted, but it gets aborted since |obs_|
  // is removed.
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(BackToBackBeginFrameSourceTest,
       DidFinishFrameThenRemoveObserverProducesNoFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get());
  source_->RemoveObserver(obs_.get());

  // Task gets cancelled so it doesn't count as a pending task.
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(BackToBackBeginFrameSourceTest,
       TogglingObserverThenDidFinishFrameProducesCorrectFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->RemoveObserver(obs_.get());

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(10));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(10));
  source_->DidFinishFrame(obs_.get());

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(10));
  // The begin frame is posted at the time when the observer was added,
  // so it ignores changes to "now" afterward.
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 1110,
                          1110 + kDeadline, kInterval);
  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->RunUntilIdle();
}

TEST_F(BackToBackBeginFrameSourceTest,
       DidFinishFrameThenTogglingObserverProducesCorrectFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get());

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(10));
  source_->RemoveObserver(obs_.get());

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(10));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(10));
  // Ticks at the time at which the observer was added, ignoring the
  // last change to "now".
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 1120,
                          1120 + kDeadline, kInterval);
  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->RunUntilIdle();
}

TEST_F(BackToBackBeginFrameSourceTest, DidFinishFrameNoObserver) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  source_->RemoveObserver(obs_.get());
  source_->DidFinishFrame(obs_.get());
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(BackToBackBeginFrameSourceTest, DidFinishFrameMultipleCallsIdempotent) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get());
  source_->DidFinishFrame(obs_.get());
  source_->DidFinishFrame(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 1100,
                          1100 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get());
  source_->DidFinishFrame(obs_.get());
  source_->DidFinishFrame(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 1200,
                          1200 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();
}

TEST_F(BackToBackBeginFrameSourceTest, DelayInPostedTaskProducesCorrectFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(obs_.get());
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(50));
  // Ticks at the time the last frame finished, so ignores the last change to
  // "now".
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 1100,
                          1100 + kDeadline, kInterval);

  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->RunUntilIdle();
}

TEST_F(BackToBackBeginFrameSourceTest, MultipleObserversSynchronized) {
  NiceMock<MockBeginFrameObserver> obs1, obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  source_->AddObserver(&obs1);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source_->AddObserver(&obs2);

  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 1, 1000, 1000 + kDeadline,
                          kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 1, 1000, 1000 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs1);
  source_->DidFinishFrame(&obs2);
  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 2, 1100, 1100 + kDeadline,
                          kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 2, 1100, 1100 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs1);
  source_->DidFinishFrame(&obs2);
  EXPECT_TRUE(task_runner_->HasPendingTask());
  source_->RemoveObserver(&obs1);
  source_->RemoveObserver(&obs2);
  task_runner_->RunUntilIdle();
}

TEST_F(BackToBackBeginFrameSourceTest, MultipleObserversInterleaved) {
  NiceMock<MockBeginFrameObserver> obs1, obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  source_->AddObserver(&obs1);
  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 1, 1000, 1000 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source_->AddObserver(&obs2);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 2, 1100, 1100 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs1);
  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 3, 1200, 1200 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  source_->DidFinishFrame(&obs1);
  source_->RemoveObserver(&obs1);
  // Removing all finished observers should disable the time source.
  EXPECT_FALSE(delay_based_time_source_->Active());
  // Finishing the frame for |obs1| posts a begin frame task, which will be
  // aborted since |obs1| is removed. Clear that from the task runner.
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs2);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 4, 1300, 1300 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  source_->DidFinishFrame(&obs2);
  source_->RemoveObserver(&obs2);
}

TEST_F(BackToBackBeginFrameSourceTest, MultipleObserversAtOnce) {
  NiceMock<MockBeginFrameObserver> obs1, obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source_->AddObserver(&obs1);
  source_->AddObserver(&obs2);
  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 1, 1000, 1000 + kDeadline,
                          kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 1, 1000, 1000 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  // |obs1| finishes first.
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs1);

  // |obs2| finishes also, before getting to the newly posted begin frame.
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(100));
  source_->DidFinishFrame(&obs2);

  // Because the begin frame source already ticked when |obs1| finished,
  // we see it as the frame time for both observers.
  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 2, 1100, 1100 + kDeadline,
                          kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 2, 1100, 1100 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  source_->DidFinishFrame(&obs1);
  source_->RemoveObserver(&obs1);
  source_->DidFinishFrame(&obs2);
  source_->RemoveObserver(&obs2);
}

// DelayBasedBeginFrameSource testing
// ------------------------------------------
class DelayBasedBeginFrameSourceTest : public ::testing::Test {
 public:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<TestTaskRunner>();
    std::unique_ptr<FakeDelayBasedTimeSource> time_source =
        std::make_unique<FakeDelayBasedTimeSource>(
            task_runner_->GetMockTickClock(), task_runner_.get());

    time_source->SetTimebaseAndInterval(
        base::TimeTicks(), base::TimeDelta::FromMicroseconds(10000));
    source_ = std::make_unique<DelayBasedBeginFrameSource>(
        std::move(time_source), BeginFrameSource::kNotRestartableId);
    obs_.reset(new MockBeginFrameObserver);
  }

  void TearDown() override { obs_.reset(); }

  scoped_refptr<TestTaskRunner> task_runner_;
  std::unique_ptr<DelayBasedBeginFrameSource> source_;
  std::unique_ptr<MockBeginFrameObserver> obs_;
};

TEST_F(DelayBasedBeginFrameSourceTest,
       AddObserverCallsOnBeginFrameWithMissedTick) {
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(9010));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 10000, 20000,
                                 10000);
  source_->AddObserver(obs_.get());  // Should cause the last tick to be sent
  // No tasks should need to be run for this to occur.
}

TEST_F(DelayBasedBeginFrameSourceTest, AddObserverCallsCausesOnBeginFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());
  EXPECT_EQ(TicksFromMicroseconds(10000),
            task_runner_->NowTicks() + task_runner_->NextPendingTaskDelay());

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(9010));
  task_runner_->RunUntilIdle();
}

TEST_F(DelayBasedBeginFrameSourceTest, BasicOperation) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 4, 30000, 40000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(30001));

  source_->RemoveObserver(obs_.get());
  // No new frames....
  task_runner_->FastForwardTo(TicksFromMicroseconds(60000));
}

TEST_F(DelayBasedBeginFrameSourceTest, VSyncChanges) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 4, 30000, 40000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(30001));

  // Update the vsync information
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(27500),
                                   base::TimeDelta::FromMicroseconds(10001));

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 5, 40000, 47502, 10001);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 6, 47502, 57503, 10001);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 7, 57503, 67504, 10001);
  task_runner_->FastForwardTo(TicksFromMicroseconds(60000));
}

TEST_F(DelayBasedBeginFrameSourceTest, VSyncChangeTimebaseBeforeLastTick) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 4, 30000, 40000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(30000));

  // Update the vsync information such that timebase is before last tick time,
  // and next tick happens within less than the new interval of the following
  // tick (i.e. next_tick -> 40000, following_tick -> 41000)
  // Begin frame won't be used at 41000 because this is a double-tick.
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(26000),
                                   base::TimeDelta::FromMicroseconds(5000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 5, 40000, 41000, 5000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 7, 46000, 51000, 5000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(46000));

  // Update the vsync information such that timebase is before last tick time,
  // and next tick happens exactly one interval before the following tick
  // tick (i.e. next_tick -> 51000, following_tick -> 60000)
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(42000),
                                   base::TimeDelta::FromMicroseconds(9000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 8, 51000, 60000, 9000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 9, 60000, 69000, 9000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 10, 69000, 78000, 9000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(70000));
}

TEST_F(DelayBasedBeginFrameSourceTest, VSyncChangeTimebaseAfterNextTick) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 4, 30000, 40000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(30000));

  // Update the vsync information such that timebase is after next tick time,
  // and next tick happens within less than one interval of the new timebase
  // Begin frame won't be used at 41000 because this is a double-tick.
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(41000),
                                   base::TimeDelta::FromMicroseconds(5000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 5, 40000, 41000, 5000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 7, 46000, 51000, 5000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(46000));

  // Update the vsync information such that timebase is after next tick time,
  // and next tick happens exactly one interval before the new timebase
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(60000),
                                   base::TimeDelta::FromMicroseconds(9000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 8, 51000, 60000, 9000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 9, 60000, 69000, 9000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 10, 69000, 78000, 9000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(70000));

  // Update the vsync information such that timebase is after next tick time,
  // and next tick happens more than one interval before the new timebase
  // Begin frame won't be used at 80000 because this is a double-tick.
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(100000),
                                   base::TimeDelta::FromMicroseconds(5000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 11, 78000, 80000, 5000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 13, 85000, 90000, 5000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(85000));
}

TEST_F(DelayBasedBeginFrameSourceTest, VSyncChangeTimebaseBetweenTicks) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 4, 30000, 40000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(30000));

  // Update the vsync information such that timebase is between next tick time,
  // and last tick time.
  // Begin frame won't be used at 41000 because this is a double-tick.
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(35000),
                                   base::TimeDelta::FromMicroseconds(6000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 5, 40000, 41000, 6000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 7, 47000, 53000, 6000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(47000));

  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(49000),
                                   base::TimeDelta::FromMicroseconds(10000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 8, 53000, 59000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 9, 59000, 69000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 10, 69000, 79000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(70000));
}

TEST_F(DelayBasedBeginFrameSourceTest, MultipleObservers) {
  NiceMock<MockBeginFrameObserver> obs1, obs2;

  // Mock tick clock starts off at 1000.
  task_runner_->FastForwardBy(base::TimeDelta::FromMicroseconds(9010));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs1, source_->source_id(), 1, 10000, 20000,
                                 10000);
  source_->AddObserver(&obs1);  // Should cause the last tick to be sent
  // No tasks should need to be run for this to occur.

  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 2, 20000, 30000, 10000);
  task_runner_->FastForwardBy(base::TimeDelta::FromMicroseconds(10000));

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  // Sequence number unchanged for missed frame with time of last normal frame.
  EXPECT_BEGIN_FRAME_USED_MISSED(obs2, source_->source_id(), 2, 20000, 30000,
                                 10000);
  source_->AddObserver(&obs2);  // Should cause the last tick to be sent
  // No tasks should need to be run for this to occur.

  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 3, 30000, 40000, 10000);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 3, 30000, 40000, 10000);
  task_runner_->FastForwardBy(base::TimeDelta::FromMicroseconds(10000));

  source_->RemoveObserver(&obs1);

  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 4, 40000, 50000, 10000);
  task_runner_->FastForwardBy(base::TimeDelta::FromMicroseconds(10000));

  source_->RemoveObserver(&obs2);
  task_runner_->FastForwardTo(TicksFromMicroseconds(50000));
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(DelayBasedBeginFrameSourceTest, DoubleTick) {
  NiceMock<MockBeginFrameObserver> obs;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 1, 0, 10000, 10000);
  source_->AddObserver(&obs);

  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(5000),
                                   base::TimeDelta::FromMicroseconds(10000));
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(4000));

  // No begin frame received.
  task_runner_->RunUntilIdle();

  // Begin frame received.
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(10000),
                                   base::TimeDelta::FromMicroseconds(10000));
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(5000));
  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 2, 10000, 20000, 10000);
  task_runner_->RunUntilIdle();
}

TEST_F(DelayBasedBeginFrameSourceTest, DoubleTickMissedFrame) {
  NiceMock<MockBeginFrameObserver> obs;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 1, 0, 10000, 10000);
  source_->AddObserver(&obs);
  source_->RemoveObserver(&obs);

  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(5000),
                                   base::TimeDelta::FromMicroseconds(10000));
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(4000));

  // No missed frame received.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  // This does not cause a missed BeginFrame because of double ticking
  // prevention. It does not produce a new sequence number.
  source_->AddObserver(&obs);
  source_->RemoveObserver(&obs);

  // Missed frame received.
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(10000),
                                   base::TimeDelta::FromMicroseconds(10000));
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(5000));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  // Sequence number is incremented again, because sufficient time has passed.
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 2, 10000, 20000,
                                 10000);
  source_->AddObserver(&obs);
  source_->RemoveObserver(&obs);
}

TEST_F(DelayBasedBeginFrameSourceTest, MultipleArgsInSameInterval) {
  NiceMock<MockBeginFrameObserver> obs;
  NiceMock<MockBeginFrameObserver> obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 1, 0, 10000, 10000);
  source_->AddObserver(&obs);
  task_runner_->RunUntilIdle();

  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 2, 10000, 20000, 10000);
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(9000));
  task_runner_->RunUntilIdle();

  // Sequence number should stay the same within same interval.
  EXPECT_BEGIN_FRAME_USED_MISSED(obs2, source_->source_id(), 2, 10000, 20000,
                                 10000);
  source_->AddObserver(&obs2);

  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 3, 20000, 30000, 10000);
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(10000));
  task_runner_->RunUntilIdle();
}

TEST_F(DelayBasedBeginFrameSourceTest, ConsecutiveArgsDelayedByMultipleVsyncs) {
  NiceMock<MockBeginFrameObserver> obs;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 1, 0, 10000, 10000);
  source_->AddObserver(&obs);
  task_runner_->RunUntilIdle();

  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 2, 10000, 20000, 10000);
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(9000));
  task_runner_->RunUntilIdle();
  source_->RemoveObserver(&obs);

  // New args created 8 intervals later.
  // Sequence number should increase bt this much.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 10, 90000, 100000,
                                 10000);
  task_runner_->AdvanceMockTickClock(base::TimeDelta::FromMicroseconds(80000));
  source_->AddObserver(&obs);
}

// ExternalBeginFrameSource testing
// --------------------------------------------
class MockExternalBeginFrameSourceClient
    : public ExternalBeginFrameSourceClient {
 public:
  MockExternalBeginFrameSourceClient() = default;
  virtual ~MockExternalBeginFrameSourceClient() = default;

  MOCK_METHOD1(OnNeedsBeginFrames, void(bool));
};

class ExternalBeginFrameSourceTest : public ::testing::Test {
 public:
  void SetUp() override {
    client_ = std::make_unique<MockExternalBeginFrameSourceClient>();
    source_ = std::make_unique<ExternalBeginFrameSource>(
        client_.get(), BeginFrameSource::kNotRestartableId);
    obs_ = std::make_unique<MockBeginFrameObserver>();
  }

  void TearDown() override {
    client_.reset();
    obs_.reset();
  }

  std::unique_ptr<MockExternalBeginFrameSourceClient> client_;
  std::unique_ptr<ExternalBeginFrameSource> source_;
  std::unique_ptr<MockBeginFrameObserver> obs_;
};

TEST_F(ExternalBeginFrameSourceTest, OnAnimateOnlyBeginFrameOptIn) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_CALL((*client_), OnNeedsBeginFrames(true)).Times(1);
  source_->AddObserver(obs_.get());

  // By default, an observer doesn't receive animate_only BeginFrames.
  BeginFrameArgs args = CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 2, TicksFromMicroseconds(10000));
  args.animate_only = true;
  source_->OnBeginFrame(args);

  // When opting in, an observer receives animate_only BeginFrames.
  args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 3,
                                        TicksFromMicroseconds(10001));
  args.animate_only = true;
  EXPECT_CALL(*obs_, WantsAnimateOnlyBeginFrames())
      .WillOnce(::testing::Return(true));
  EXPECT_BEGIN_FRAME_ARGS_USED(*obs_, args);
  source_->OnBeginFrame(args);

  EXPECT_CALL((*client_), OnNeedsBeginFrames(false)).Times(1);
  source_->RemoveObserver(obs_.get());
}

TEST_F(ExternalBeginFrameSourceTest, OnBeginFrameChecksBeginFrameContinuity) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_CALL((*client_), OnNeedsBeginFrames(true)).Times(1);
  source_->AddObserver(obs_.get());

  BeginFrameArgs args = CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 2, TicksFromMicroseconds(10000));
  EXPECT_BEGIN_FRAME_ARGS_USED(*obs_, args);
  source_->OnBeginFrame(args);

  // Providing same args again to OnBeginFrame() should not notify observer.
  source_->OnBeginFrame(args);

  // Providing same args through a different ExternalBeginFrameSource also
  // does not notify observer.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_CALL((*client_), OnNeedsBeginFrames(true)).Times(1);
  ExternalBeginFrameSource source2(client_.get());
  source2.AddObserver(obs_.get());
  source2.OnBeginFrame(args);

  EXPECT_CALL((*client_), OnNeedsBeginFrames(false)).Times(2);
  source_->RemoveObserver(obs_.get());
  source2.RemoveObserver(obs_.get());
}

TEST_F(ExternalBeginFrameSourceTest, GetMissedBeginFrameArgs) {
  BeginFrameArgs args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0,
                                                       2, 10000, 10100, 100);
  source_->OnBeginFrame(args);

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, 0, 2, 10000, 10100, 100);
  source_->AddObserver(obs_.get());
  source_->RemoveObserver(obs_.get());

  // Out of order frame_time. This might not be valid but still shouldn't
  // cause a DCHECK in ExternalBeginFrameSource code.
  args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2, 9999, 10100,
                                        101);
  source_->OnBeginFrame(args);

  EXPECT_CALL((*client_), OnNeedsBeginFrames(true)).Times(1);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_CALL(*obs_, OnBeginFrame(_)).Times(0);
  source_->AddObserver(obs_.get());

  EXPECT_CALL((*client_), OnNeedsBeginFrames(false)).Times(1);
  source_->RemoveObserver(obs_.get());
}

// Tests that an observer which returns true from IsRoot is notified after
// observers which return false.
TEST_F(ExternalBeginFrameSourceTest, RootsNotifiedLast) {
  using ::testing::InSequence;

  NiceMock<MockBeginFrameObserver> obs1, obs2;
  source_->AddObserver(&obs1);
  source_->AddObserver(&obs2);

  {
    BeginFrameArgs args = CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, 0, 1, 10000, 10100, 100);
    // Set obs1 to root, obs2 to child.
    EXPECT_CALL(obs1, IsRoot()).WillRepeatedly(::testing::Return(true));
    EXPECT_CALL(obs2, IsRoot()).WillRepeatedly(::testing::Return(false));
    {
      // Ensure that OnBeginFrame delivers the calls in the right order.
      InSequence s;
      EXPECT_CALL(obs2, OnBeginFrame(args))
          .WillOnce(::testing::SaveArg<0>(&(obs2.last_begin_frame_args)));
      EXPECT_CALL(obs1, OnBeginFrame(args))
          .WillOnce(::testing::SaveArg<0>(&(obs1.last_begin_frame_args)));
      source_->OnBeginFrame(args);
    }
  }

  {
    BeginFrameArgs args = CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, 0, 2, 10001, 10101, 100);
    // Set obs2 to root, obs1 to child.
    EXPECT_CALL(obs1, IsRoot()).WillRepeatedly(::testing::Return(false));
    EXPECT_CALL(obs2, IsRoot()).WillRepeatedly(::testing::Return(true));
    {
      // Ensure that OnBeginFrame delivers the calls in the right order.
      InSequence s;
      EXPECT_CALL(obs1, OnBeginFrame(args))
          .WillOnce(::testing::SaveArg<0>(&(obs1.last_begin_frame_args)));
      EXPECT_CALL(obs2, OnBeginFrame(args))
          .WillOnce(::testing::SaveArg<0>(&(obs2.last_begin_frame_args)));
      source_->OnBeginFrame(args);
    }
  }

  source_->RemoveObserver(&obs1);
  source_->RemoveObserver(&obs2);
}

}  // namespace
}  // namespace viz
