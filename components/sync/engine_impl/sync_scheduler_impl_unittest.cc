// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/sync_scheduler_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/base/cancelation_signal.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/model_type_test_util.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/engine_impl/backoff_delay_provider.h"
#include "components/sync/engine_impl/cycle/test_util.h"
#include "components/sync/syncable/test_user_share.h"
#include "components/sync/test/callback_counter.h"
#include "components/sync/test/engine/fake_model_worker.h"
#include "components/sync/test/engine/mock_connection_manager.h"
#include "components/sync/test/engine/mock_nudge_handler.h"
#include "components/sync/test/mock_invalidation.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::Eq;
using testing::Ge;
using testing::Gt;
using testing::Invoke;
using testing::Lt;
using testing::Mock;
using testing::Return;
using testing::SaveArg;
using testing::WithArg;
using testing::WithArgs;
using testing::WithoutArgs;

namespace syncer {

class MockSyncer : public Syncer {
 public:
  MockSyncer();
  MOCK_METHOD3(NormalSyncShare, bool(ModelTypeSet, NudgeTracker*, SyncCycle*));
  MOCK_METHOD3(ConfigureSyncShare,
               bool(const ModelTypeSet&,
                    sync_pb::SyncEnums::GetUpdatesOrigin,
                    SyncCycle*));
  MOCK_METHOD2(PollSyncShare, bool(ModelTypeSet, SyncCycle*));
};

MockSyncer::MockSyncer() : Syncer(nullptr) {}

using SyncShareTimes = std::vector<TimeTicks>;

void QuitLoopNow() {
  // We use QuitNow() instead of Quit() as the latter may get stalled
  // indefinitely in the presence of repeated timers with low delays
  // and a slow test (e.g., ThrottlingDoesThrottle [which has a poll
  // delay of 5ms] run under TSAN on the trybots).
  base::RunLoop::QuitCurrentDeprecated();
}

void RunLoop() {
  base::RunLoop().Run();
}

void PumpLoop() {
  // Do it this way instead of RunAllPending to pump loop exactly once
  // (necessary in the presence of timers; see comment in
  // QuitLoopNow).
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&QuitLoopNow));
  RunLoop();
}

static const size_t kMinNumSamples = 5;

// Test harness for the SyncScheduler.  Test the delays and backoff timers used
// in response to various events.  Mock time is used to avoid flakes.
class SyncSchedulerImplTest : public testing::Test {
 public:
  SyncSchedulerImplTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::ThreadPoolExecutionMode::
                ASYNC,
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME),
        syncer_(nullptr),
        delay_(nullptr) {}

  class MockDelayProvider : public BackoffDelayProvider {
   public:
    MockDelayProvider()
        : BackoffDelayProvider(
              TimeDelta::FromSeconds(kInitialBackoffRetrySeconds),
              TimeDelta::FromSeconds(kInitialBackoffImmediateRetrySeconds)) {}

    MOCK_METHOD1(GetDelay, TimeDelta(const TimeDelta&));
  };

  void SetUp() override {
    test_user_share_.SetUp();
    delay_ = nullptr;
    extensions_activity_ = new ExtensionsActivity();

    workers_.clear();
    workers_.push_back(base::MakeRefCounted<FakeModelWorker>(GROUP_UI));
    workers_.push_back(base::MakeRefCounted<FakeModelWorker>(GROUP_PASSIVE));

    connection_ = std::make_unique<MockConnectionManager>(directory());
    connection_->SetServerReachable();

    model_type_registry_ = std::make_unique<ModelTypeRegistry>(
        workers_, test_user_share_.user_share(), &mock_nudge_handler_,
        UssMigrator(), &cancelation_signal_,
        test_user_share_.keystore_keys_handler());
    model_type_registry_->RegisterDirectoryType(HISTORY_DELETE_DIRECTIVES,
                                                GROUP_UI);
    model_type_registry_->RegisterDirectoryType(NIGORI, GROUP_PASSIVE);
    model_type_registry_->RegisterDirectoryType(THEMES, GROUP_UI);
    model_type_registry_->RegisterDirectoryType(TYPED_URLS, GROUP_UI);

    context_ = std::make_unique<SyncCycleContext>(
        connection_.get(), directory(), extensions_activity_.get(),
        std::vector<SyncEngineEventListener*>(), nullptr,
        model_type_registry_.get(), "fake_invalidator_client_id",
        "fake_birthday", "fake_bag_of_chips",
        /*poll_interval=*/base::TimeDelta::FromMinutes(30));
    context_->set_notifications_enabled(true);
    context_->set_account_name("Test");
    RebuildScheduler();
  }

  void UnregisterDataType(ModelType type) {
    model_type_registry_->UnregisterDirectoryType(type);
  }

  void RebuildScheduler() {
    // The old syncer is destroyed with the scheduler that owns it.
    syncer_ = new testing::StrictMock<MockSyncer>();
    scheduler_ = std::make_unique<SyncSchedulerImpl>(
        "TestSyncScheduler", BackoffDelayProvider::FromDefaults(), context(),
        syncer_, false);
    scheduler_->nudge_tracker_.SetDefaultNudgeDelay(default_delay());
  }

  SyncSchedulerImpl* scheduler() { return scheduler_.get(); }
  MockSyncer* syncer() { return syncer_; }
  MockDelayProvider* delay() { return delay_; }
  MockConnectionManager* connection() { return connection_.get(); }
  TimeDelta default_delay() { return TimeDelta::FromSeconds(0); }
  TimeDelta long_delay() { return TimeDelta::FromSeconds(60); }
  TimeDelta timeout() { return TestTimeouts::action_timeout(); }

  void TearDown() override {
    PumpLoop();
    scheduler_.reset();
    PumpLoop();
    test_user_share_.TearDown();
  }

  void AnalyzePollRun(const SyncShareTimes& times,
                      size_t min_num_samples,
                      const TimeTicks& optimal_start,
                      const TimeDelta& poll_interval) {
    EXPECT_GE(times.size(), min_num_samples);
    for (size_t i = 0; i < times.size(); i++) {
      SCOPED_TRACE(testing::Message() << "SyncShare # (" << i << ")");
      TimeTicks optimal_next_sync = optimal_start + poll_interval * i;
      EXPECT_GE(times[i], optimal_next_sync);
    }
  }

  void DoQuitLoopNow() { QuitLoopNow(); }

  void StartSyncConfiguration() {
    scheduler()->Start(SyncScheduler::CONFIGURATION_MODE, base::Time());
  }

  void StartSyncScheduler(base::Time last_poll_time) {
    scheduler()->Start(SyncScheduler::NORMAL_MODE, last_poll_time);
  }

  // This stops the scheduler synchronously.
  void StopSyncScheduler() {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SyncSchedulerImplTest::DoQuitLoopNow,
                                  weak_ptr_factory_.GetWeakPtr()));
    RunLoop();
  }

  bool RunAndGetBackoff() {
    ModelTypeSet nudge_types(THEMES);
    StartSyncScheduler(base::Time());

    scheduler()->ScheduleLocalNudge(nudge_types, FROM_HERE);
    RunLoop();

    return scheduler()->IsGlobalBackoff();
  }

  void UseMockDelayProvider() {
    delay_ = new MockDelayProvider();
    scheduler_->delay_provider_.reset(delay_);
  }

  SyncCycleContext* context() { return context_.get(); }

  ModelTypeSet GetThrottledTypes() {
    ModelTypeSet throttled_types;
    ModelTypeSet blocked_types = scheduler_->nudge_tracker_.GetBlockedTypes();
    for (ModelType type : blocked_types) {
      if (scheduler_->nudge_tracker_.GetTypeBlockingMode(type) ==
          WaitInterval::THROTTLED) {
        throttled_types.Put(type);
      }
    }
    return throttled_types;
  }

  ModelTypeSet GetBackedOffTypes() {
    ModelTypeSet backed_off_types;
    ModelTypeSet blocked_types = scheduler_->nudge_tracker_.GetBlockedTypes();
    for (ModelType type : blocked_types) {
      if (scheduler_->nudge_tracker_.GetTypeBlockingMode(type) ==
          WaitInterval::EXPONENTIAL_BACKOFF) {
        backed_off_types.Put(type);
      }
    }
    return backed_off_types;
  }

  bool IsAnyTypeBlocked() {
    return scheduler_->nudge_tracker_.IsAnyTypeBlocked();
  }

  TimeDelta GetRetryTimerDelay() {
    EXPECT_TRUE(scheduler_->retry_timer_.IsRunning());
    return scheduler_->retry_timer_.GetCurrentDelay();
  }

  static std::unique_ptr<InvalidationInterface> BuildInvalidation(
      int64_t version,
      const std::string& payload) {
    return MockInvalidation::Build(version, payload);
  }

  TimeDelta GetTypeBlockingTime(ModelType type) {
    NudgeTracker::TypeTrackerMap::const_iterator tracker_it =
        scheduler_->nudge_tracker_.type_trackers_.find(type);
    DCHECK(tracker_it != scheduler_->nudge_tracker_.type_trackers_.end());
    DCHECK(tracker_it->second->wait_interval_);
    return tracker_it->second->wait_interval_->length;
  }

  void SetTypeBlockingMode(ModelType type, WaitInterval::BlockingMode mode) {
    NudgeTracker::TypeTrackerMap::const_iterator tracker_it =
        scheduler_->nudge_tracker_.type_trackers_.find(type);
    DCHECK(tracker_it != scheduler_->nudge_tracker_.type_trackers_.end());
    DCHECK(tracker_it->second->wait_interval_);
    tracker_it->second->wait_interval_->mode = mode;
  }

  void NewSchedulerForLocalBackend() {
    // The old syncer is destroyed with the scheduler that owns it.
    syncer_ = new testing::StrictMock<MockSyncer>();
    scheduler_ = std::make_unique<SyncSchedulerImpl>(
        "TestSyncScheduler", BackoffDelayProvider::FromDefaults(), context(),
        syncer_, true);
    scheduler_->nudge_tracker_.SetDefaultNudgeDelay(default_delay());
  }

  bool BlockTimerIsRunning() const {
    return scheduler_->pending_wakeup_timer_.IsRunning();
  }

  TimeDelta GetPendingWakeupTimerDelay() {
    EXPECT_TRUE(scheduler_->pending_wakeup_timer_.IsRunning());
    return scheduler_->pending_wakeup_timer_.GetCurrentDelay();
  }

  // Provide access for tests to private method.
  base::Time ComputeLastPollOnStart(base::Time last_poll,
                                    base::TimeDelta poll_interval,
                                    base::Time now) {
    return SyncSchedulerImpl::ComputeLastPollOnStart(last_poll, poll_interval,
                                                     now);
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

 private:
  static const base::TickClock* tick_clock_;
  static base::TimeTicks GetMockTimeTicks() {
    if (!tick_clock_)
      return base::TimeTicks();
    return tick_clock_->NowTicks();
  }

  syncable::Directory* directory() {
    return test_user_share_.user_share()->directory.get();
  }

  TestUserShare test_user_share_;
  CancelationSignal cancelation_signal_;
  std::unique_ptr<MockConnectionManager> connection_;
  std::unique_ptr<ModelTypeRegistry> model_type_registry_;
  std::unique_ptr<SyncCycleContext> context_;
  std::unique_ptr<SyncSchedulerImpl> scheduler_;
  MockNudgeHandler mock_nudge_handler_;
  MockSyncer* syncer_;
  MockDelayProvider* delay_;
  std::vector<scoped_refptr<ModelSafeWorker>> workers_;
  scoped_refptr<ExtensionsActivity> extensions_activity_;
  base::WeakPtrFactory<SyncSchedulerImplTest> weak_ptr_factory_{this};
};

const base::TickClock* SyncSchedulerImplTest::tick_clock_ = nullptr;

void RecordSyncShareImpl(SyncShareTimes* times) {
  times->push_back(TimeTicks::Now());
}

ACTION_P2(RecordSyncShare, times, success) {
  RecordSyncShareImpl(times);
  if (base::RunLoop::IsRunningOnCurrentThread())
    QuitLoopNow();
  return success;
}

ACTION_P3(RecordSyncShareMultiple, times, quit_after, success) {
  RecordSyncShareImpl(times);
  EXPECT_LE(times->size(), quit_after);
  if (times->size() >= quit_after &&
      base::RunLoop::IsRunningOnCurrentThread()) {
    QuitLoopNow();
  }
  return success;
}

ACTION_P(StopScheduler, scheduler) {
  scheduler->Stop();
}

ACTION(AddFailureAndQuitLoopNow) {
  ADD_FAILURE();
  QuitLoopNow();
  return true;
}

ACTION_P(QuitLoopNowAction, success) {
  QuitLoopNow();
  return success;
}

// Test nudge scheduling.
TEST_F(SyncSchedulerImplTest, Nudge) {
  SyncShareTimes times;
  ModelTypeSet model_types(THEMES);

  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());

  scheduler()->ScheduleLocalNudge(model_types, FROM_HERE);
  RunLoop();

  Mock::VerifyAndClearExpectations(syncer());

  // Make sure a second, later, nudge is unaffected by first (no coalescing).
  SyncShareTimes times2;
  model_types.Remove(THEMES);
  model_types.Put(TYPED_URLS);
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times2, true)));
  scheduler()->ScheduleLocalNudge(model_types, FROM_HERE);
  RunLoop();
}

TEST_F(SyncSchedulerImplTest, NudgeForDisabledType) {
  ModelTypeSet model_types{THEMES, HISTORY_DELETE_DIRECTIVES};

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(model_types, FROM_HERE);

  // The user enables a custom passphrase at this point, so
  // HISTORY_DELETE_DIRECTIVES gets disabled.
  UnregisterDataType(HISTORY_DELETE_DIRECTIVES);
  ASSERT_FALSE(context()->GetEnabledTypes().Has(HISTORY_DELETE_DIRECTIVES));

  // The resulting sync cycle should ask only for the remaining types.
  SyncShareTimes times;
  NudgeTracker* nudge_tracker = nullptr;
  EXPECT_CALL(*syncer(), NormalSyncShare(context()->GetEnabledTypes(), _, _))
      .WillOnce(DoAll(SaveArg<1>(&nudge_tracker),
                      Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)));
  RunLoop();

  // Now no sync is required for the enabled types.
  ASSERT_FALSE(nudge_tracker->IsSyncRequired(context()->GetEnabledTypes()));
  // ...but HISTORY_DELETE_DIRECTIVES is not enabled, so its earlier nudge is
  // still there.
  EXPECT_TRUE(nudge_tracker->IsSyncRequired({HISTORY_DELETE_DIRECTIVES}));
}

// Make sure a regular config command is scheduled fine in the absence of any
// errors.
TEST_F(SyncSchedulerImplTest, Config) {
  SyncShareTimes times;
  const ModelTypeSet model_types(THEMES);

  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)));

  StartSyncConfiguration();

  CallbackCounter ready_counter;
  ConfigurationParams params(
      sync_pb::SyncEnums::RECONFIGURATION, model_types,
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)));
  scheduler()->ScheduleConfiguration(params);
  PumpLoop();
  ASSERT_EQ(1, ready_counter.times_called());
}

// Simulate a failure and make sure the config request is retried.
TEST_F(SyncSchedulerImplTest, ConfigWithBackingOff) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(20)));
  SyncShareTimes times;
  const ModelTypeSet model_types(THEMES);

  StartSyncConfiguration();

  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateConfigureFailed),
                      RecordSyncShare(&times, false)))
      .WillOnce(DoAll(Invoke(test_util::SimulateConfigureFailed),
                      RecordSyncShare(&times, false)));

  CallbackCounter ready_counter;
  ConfigurationParams params(
      sync_pb::SyncEnums::RECONFIGURATION, model_types,
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)));
  scheduler()->ScheduleConfiguration(params);
  RunLoop();
  ASSERT_EQ(0, ready_counter.times_called());

  // RunLoop() will trigger TryCanaryJob which will retry configuration.
  // Since retry_task was already called it shouldn't be called again.
  RunLoop();
  ASSERT_EQ(0, ready_counter.times_called());

  Mock::VerifyAndClearExpectations(syncer());

  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)));
  RunLoop();

  ASSERT_EQ(1, ready_counter.times_called());
}

// Simuilate SyncSchedulerImpl::Stop being called in the middle of Configure.
// This can happen if server returns NOT_MY_BIRTHDAY.
TEST_F(SyncSchedulerImplTest, ConfigWithStop) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(20)));
  SyncShareTimes times;
  const ModelTypeSet model_types(THEMES);

  StartSyncConfiguration();

  // Make ConfigureSyncShare call scheduler->Stop(). It is not supposed to call
  // retry_task or dereference configuration params.
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateConfigureFailed),
                      StopScheduler(scheduler()),
                      RecordSyncShare(&times, false)));

  CallbackCounter ready_counter;
  ConfigurationParams params(
      sync_pb::SyncEnums::RECONFIGURATION, model_types,
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)));
  scheduler()->ScheduleConfiguration(params);
  PumpLoop();
  ASSERT_EQ(0, ready_counter.times_called());
}

// Verify that in the absence of valid access token the command will fail.
TEST_F(SyncSchedulerImplTest, ConfigNoAccessToken) {
  SyncShareTimes times;
  const ModelTypeSet model_types(THEMES);

  connection()->ResetAccessToken();

  StartSyncConfiguration();

  CallbackCounter ready_counter;
  ConfigurationParams params(
      sync_pb::SyncEnums::RECONFIGURATION, model_types,
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)));
  scheduler()->ScheduleConfiguration(params);
  PumpLoop();
  ASSERT_EQ(0, ready_counter.times_called());
}

// Verify that in the absence of valid access token the command will pass if
// local sync backend is used.
TEST_F(SyncSchedulerImplTest, ConfigNoAccessTokenLocalSync) {
  SyncShareTimes times;
  const ModelTypeSet model_types(THEMES);

  NewSchedulerForLocalBackend();
  connection()->ResetAccessToken();

  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)));

  StartSyncConfiguration();

  CallbackCounter ready_counter;
  ConfigurationParams params(
      sync_pb::SyncEnums::RECONFIGURATION, model_types,
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)));
  scheduler()->ScheduleConfiguration(params);
  PumpLoop();
  ASSERT_EQ(1, ready_counter.times_called());
}

// Issue a nudge when the config has failed. Make sure both the config and
// nudge are executed.
TEST_F(SyncSchedulerImplTest, NudgeWithConfigWithBackingOff) {
  const ModelTypeSet model_types(THEMES);
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(50)));
  SyncShareTimes times;

  StartSyncConfiguration();

  // Request a configure and make sure it fails.
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateConfigureFailed),
                      RecordSyncShare(&times, false)));
  CallbackCounter ready_counter;
  ConfigurationParams params(
      sync_pb::SyncEnums::RECONFIGURATION, model_types,
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)));
  scheduler()->ScheduleConfiguration(params);
  RunLoop();
  ASSERT_EQ(0, ready_counter.times_called());
  Mock::VerifyAndClearExpectations(syncer());

  // Ask for a nudge while dealing with repeated configure failure.
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateConfigureFailed),
                      RecordSyncShare(&times, false)));
  scheduler()->ScheduleLocalNudge(model_types, FROM_HERE);
  RunLoop();
  // Note that we're not RunLoop()ing for the NUDGE we just scheduled, but
  // for the first retry attempt from the config job (after
  // waiting ~+/- 50ms).
  Mock::VerifyAndClearExpectations(syncer());
  ASSERT_EQ(0, ready_counter.times_called());

  // Let the next configure retry succeed.
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)));
  RunLoop();

  // Now change the mode so nudge can execute.
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)));
  StartSyncScheduler(base::Time());
  PumpLoop();
}

// Test that nudges are coalesced.
TEST_F(SyncSchedulerImplTest, NudgeCoalescing) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)));
  const ModelTypeSet types1(THEMES), types2(TYPED_URLS), types3(THEMES);
  TimeTicks optimal_time = TimeTicks::Now() + default_delay();
  scheduler()->ScheduleLocalNudge(types1, FROM_HERE);
  scheduler()->ScheduleLocalNudge(types2, FROM_HERE);
  RunLoop();

  ASSERT_EQ(1U, times.size());
  EXPECT_GE(times[0], optimal_time);

  Mock::VerifyAndClearExpectations(syncer());

  SyncShareTimes times2;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times2, true)));
  scheduler()->ScheduleLocalNudge(types3, FROM_HERE);
  RunLoop();
}

// Test that nudges are coalesced.
TEST_F(SyncSchedulerImplTest, NudgeCoalescingWithDifferentTimings) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)));
  ModelTypeSet types1(THEMES), types2(TYPED_URLS), types3;

  // Create a huge time delay.
  TimeDelta delay = TimeDelta::FromDays(1);

  std::map<ModelType, TimeDelta> delay_map;
  delay_map[*(types1.begin())] = delay;
  scheduler()->OnReceivedCustomNudgeDelays(delay_map);
  scheduler()->ScheduleLocalNudge(types1, FROM_HERE);
  scheduler()->ScheduleLocalNudge(types2, FROM_HERE);

  TimeTicks min_time = TimeTicks::Now();
  TimeTicks max_time = TimeTicks::Now() + delay;

  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());

  // Make sure the sync happened at the right time.
  ASSERT_EQ(1U, times.size());
  EXPECT_GE(times[0], min_time);
  EXPECT_LE(times[0], max_time);
}

// Test nudge scheduling.
TEST_F(SyncSchedulerImplTest, NudgeWithStates) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times1;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times1, true)))
      .RetiresOnSaturation();
  scheduler()->ScheduleInvalidationNudge(THEMES, BuildInvalidation(10, "test"),
                                         FROM_HERE);
  RunLoop();

  Mock::VerifyAndClearExpectations(syncer());

  // Make sure a second, later, nudge is unaffected by first (no coalescing).
  SyncShareTimes times2;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times2, true)));
  scheduler()->ScheduleInvalidationNudge(
      TYPED_URLS, BuildInvalidation(10, "test2"), FROM_HERE);
  RunLoop();
}

// Test that polling works as expected.
TEST_F(SyncSchedulerImplTest, Polling) {
  SyncShareTimes times;
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(30));
  EXPECT_CALL(*syncer(), PollSyncShare(_, _))
      .Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(
          DoAll(Invoke(test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  scheduler()->OnReceivedPollIntervalUpdate(poll_interval);

  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(base::Time());

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll_interval);
}

// Test that polling gets the intervals from the provided context.
TEST_F(SyncSchedulerImplTest, ShouldUseInitialPollIntervalFromContext) {
  SyncShareTimes times;
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(30));
  context()->set_poll_interval(poll_interval);
  RebuildScheduler();

  EXPECT_CALL(*syncer(), PollSyncShare(_, _))
      .Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(
          DoAll(Invoke(test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(base::Time());

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll_interval);
}

// Test that we reuse the previous poll time on startup, triggering the first
// poll based on when the last one happened. Subsequent polls should have the
// normal delay.
TEST_F(SyncSchedulerImplTest, PollingPersistence) {
  SyncShareTimes times;
  // Use a large poll interval that wouldn't normally get hit on its own for
  // some time yet.
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(500));
  EXPECT_CALL(*syncer(), PollSyncShare(_, _))
      .Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(
          DoAll(Invoke(test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  scheduler()->OnReceivedPollIntervalUpdate(poll_interval);

  // Set the start time to now, as the poll was overdue.
  TimeTicks optimal_start = TimeTicks::Now();
  StartSyncScheduler(base::Time::Now() - poll_interval);

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll_interval);
}

// Test that if the persisted poll is in the future, it's ignored (the case
// where the local time has been modified).
TEST_F(SyncSchedulerImplTest, PollingPersistenceBadClock) {
  SyncShareTimes times;
  TimeDelta poll_interval(TimeDelta::FromMilliseconds(30));
  EXPECT_CALL(*syncer(), PollSyncShare(_, _))
      .Times(AtLeast(kMinNumSamples))
      .WillRepeatedly(
          DoAll(Invoke(test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  scheduler()->OnReceivedPollIntervalUpdate(poll_interval);

  // Set the start time to |poll_interval| in the future.
  TimeTicks optimal_start = TimeTicks::Now() + poll_interval;
  StartSyncScheduler(base::Time::Now() + TimeDelta::FromMinutes(10));

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll_interval);
}

// Test that polling intervals are updated when needed.
TEST_F(SyncSchedulerImplTest, PollIntervalUpdate) {
  SyncShareTimes times;
  TimeDelta poll1(TimeDelta::FromMilliseconds(120));
  TimeDelta poll2(TimeDelta::FromMilliseconds(30));
  scheduler()->OnReceivedPollIntervalUpdate(poll1);
  EXPECT_CALL(*syncer(), PollSyncShare(_, _))
      .Times(AtLeast(kMinNumSamples))
      .WillOnce(
          DoAll(WithArgs<0, 1>(test_util::SimulatePollIntervalUpdate(poll2)),
                Return(true)))
      .WillRepeatedly(DoAll(
          Invoke(test_util::SimulatePollSuccess),
          WithArg<1>(RecordSyncShareMultiple(&times, kMinNumSamples, true))));

  TimeTicks optimal_start = TimeTicks::Now() + poll1 + poll2;
  StartSyncScheduler(base::Time());

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll2);
}

// Test that no syncing occurs when throttled.
TEST_F(SyncSchedulerImplTest, ThrottlingDoesThrottle) {
  const ModelTypeSet types(THEMES);
  TimeDelta poll(TimeDelta::FromMilliseconds(20));
  TimeDelta throttle(TimeDelta::FromMinutes(10));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(WithArg<2>(test_util::SimulateThrottled(throttle)),
                      Return(false)))
      .WillRepeatedly(AddFailureAndQuitLoopNow());

  StartSyncScheduler(base::Time());

  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop();

  StartSyncConfiguration();

  CallbackCounter ready_counter;
  ConfigurationParams params(
      sync_pb::SyncEnums::RECONFIGURATION, types,
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)));
  scheduler()->ScheduleConfiguration(params);
  PumpLoop();
  ASSERT_EQ(0, ready_counter.times_called());
}

TEST_F(SyncSchedulerImplTest, ThrottlingExpiresFromPoll) {
  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromMilliseconds(15));
  TimeDelta throttle1(TimeDelta::FromMilliseconds(150));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), PollSyncShare(_, _))
      .WillOnce(DoAll(WithArg<1>(test_util::SimulateThrottled(throttle1)),
                      Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*syncer(), PollSyncShare(_, _))
      .WillRepeatedly(
          DoAll(Invoke(test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  TimeTicks optimal_start = TimeTicks::Now() + poll + throttle1;
  StartSyncScheduler(base::Time());

  // Run again to wait for polling.
  RunLoop();

  StopSyncScheduler();
  AnalyzePollRun(times, kMinNumSamples, optimal_start, poll);
}

TEST_F(SyncSchedulerImplTest, ThrottlingExpiresFromNudge) {
  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromDays(1));
  TimeDelta throttle1(TimeDelta::FromMilliseconds(150));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(WithArg<2>(test_util::SimulateThrottled(throttle1)),
                      Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      QuitLoopNowAction(true)));

  const ModelTypeSet types(THEMES);
  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);

  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(scheduler()->IsGlobalThrottle());
  RunLoop();
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, ThrottlingExpiresFromConfigure) {
  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromDays(1));
  TimeDelta throttle1(TimeDelta::FromMilliseconds(150));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(WithArg<2>(test_util::SimulateThrottled(throttle1)),
                      Return(false)))
      .RetiresOnSaturation();
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateConfigureSuccess),
                      QuitLoopNowAction(true)));

  const ModelTypeSet types(THEMES);
  StartSyncConfiguration();

  CallbackCounter ready_counter;
  ConfigurationParams params(
      sync_pb::SyncEnums::RECONFIGURATION, types,
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)));
  scheduler()->ScheduleConfiguration(params);
  PumpLoop();
  EXPECT_EQ(0, ready_counter.times_called());
  EXPECT_TRUE(scheduler()->IsGlobalThrottle());

  RunLoop();
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeThrottlingBlocksNudge) {
  TimeDelta poll(TimeDelta::FromDays(1));
  TimeDelta throttle1(TimeDelta::FromSeconds(60));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelTypeSet types(THEMES);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(
          DoAll(WithArg<2>(test_util::SimulateTypesThrottled(types, throttle1)),
                Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetThrottledTypes().HasAll(types));
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // This won't cause a sync cycle because the types are throttled.
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeBackingOffBlocksNudge) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_)).WillRepeatedly(Return(long_delay()));

  TimeDelta poll(TimeDelta::FromDays(1));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelTypeSet types(THEMES);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(WithArg<2>(test_util::SimulatePartialFailure(types)),
                      Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().HasAll(types));
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // This won't cause a sync cycle because the types are backed off.
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeBackingOffWillExpire) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_)).WillRepeatedly(Return(default_delay()));

  TimeDelta poll(TimeDelta::FromDays(1));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelTypeSet types(THEMES);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(WithArg<2>(test_util::SimulatePartialFailure(types)),
                      Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().HasAll(types));
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillRepeatedly(DoAll(Invoke(test_util::SimulateNormalSuccess),
                            RecordSyncShare(&times, true)));
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_FALSE(IsAnyTypeBlocked());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeBackingOffAndThrottling) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_)).WillRepeatedly(Return(long_delay()));

  TimeDelta poll(TimeDelta::FromDays(1));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelTypeSet types(THEMES);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(WithArg<2>(test_util::SimulatePartialFailure(types)),
                      Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().HasAll(types));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  TimeDelta throttle1(TimeDelta::FromMilliseconds(150));

  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(WithArg<2>(test_util::SimulateThrottled(throttle1)),
                      Return(false)))
      .RetiresOnSaturation();

  // Sync still can throttle.
  const ModelTypeSet unbacked_off_types(TYPED_URLS);
  scheduler()->ScheduleLocalNudge(unbacked_off_types, FROM_HERE);
  PumpLoop();  // TO get TypesUnblock called.
  PumpLoop();  // To get TrySyncCycleJob called.

  EXPECT_TRUE(GetBackedOffTypes().HasAll(types));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_TRUE(scheduler()->IsGlobalThrottle());

  // Unthrottled client, but the backingoff datatype is still in backoff and
  // scheduled.
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      QuitLoopNowAction(true)));
  RunLoop();
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());
  EXPECT_TRUE(GetBackedOffTypes().HasAll(types));
  EXPECT_TRUE(BlockTimerIsRunning());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeThrottlingBackingOffBlocksNudge) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_)).WillRepeatedly(Return(long_delay()));

  TimeDelta poll(TimeDelta::FromDays(1));
  TimeDelta throttle(TimeDelta::FromSeconds(60));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelTypeSet throttled_types(THEMES);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(WithArg<2>(test_util::SimulateTypesThrottled(
                          throttled_types, throttle)),
                      Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(throttled_types, FROM_HERE);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called

  const ModelTypeSet backed_off_types(TYPED_URLS);

  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(
          DoAll(WithArg<2>(test_util::SimulatePartialFailure(backed_off_types)),
                Return(true)))
      .RetiresOnSaturation();

  scheduler()->ScheduleLocalNudge(backed_off_types, FROM_HERE);

  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called

  EXPECT_TRUE(GetThrottledTypes().HasAll(throttled_types));
  EXPECT_TRUE(GetBackedOffTypes().HasAll(backed_off_types));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // This won't cause a sync cycle because the types are throttled or backed
  // off.
  scheduler()->ScheduleLocalNudge(Union(throttled_types, backed_off_types),
                                  FROM_HERE);
  PumpLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeThrottlingDoesBlockOtherSources) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_)).WillRepeatedly(Return(default_delay()));

  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromDays(1));
  TimeDelta throttle1(TimeDelta::FromSeconds(60));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelTypeSet throttled_types(THEMES);
  const ModelTypeSet unthrottled_types(PREFERENCES);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(WithArg<2>(test_util::SimulateTypesThrottled(
                          throttled_types, throttle1)),
                      Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(throttled_types, FROM_HERE);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetThrottledTypes().HasAll(throttled_types));
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // Ignore invalidations for throttled types.
  scheduler()->ScheduleInvalidationNudge(THEMES, BuildInvalidation(10, "test"),
                                         FROM_HERE);
  PumpLoop();

  // Ignore refresh requests for throttled types.
  scheduler()->ScheduleLocalRefreshRequest(throttled_types, FROM_HERE);
  PumpLoop();

  Mock::VerifyAndClearExpectations(syncer());

  // Local nudges for non-throttled types will trigger a sync.
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillRepeatedly(DoAll(Invoke(test_util::SimulateNormalSuccess),
                            RecordSyncShare(&times, true)));
  scheduler()->ScheduleLocalNudge(unthrottled_types, FROM_HERE);
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, TypeBackingOffDoesBlockOtherSources) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_)).WillRepeatedly(Return(long_delay()));

  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromDays(1));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelTypeSet backed_off_types(THEMES);
  const ModelTypeSet unbacked_off_types(PREFERENCES);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(
          DoAll(WithArg<2>(test_util::SimulatePartialFailure(backed_off_types)),
                Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(backed_off_types, FROM_HERE);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().HasAll(backed_off_types));
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // Ignore invalidations for backed off types.
  scheduler()->ScheduleInvalidationNudge(THEMES, BuildInvalidation(10, "test"),
                                         FROM_HERE);
  PumpLoop();

  // Ignore refresh requests for backed off types.
  scheduler()->ScheduleLocalRefreshRequest(backed_off_types, FROM_HERE);
  PumpLoop();

  Mock::VerifyAndClearExpectations(syncer());

  // Local nudges for non-backed off types will trigger a sync.
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillRepeatedly(DoAll(Invoke(test_util::SimulateNormalSuccess),
                            RecordSyncShare(&times, true)));
  scheduler()->ScheduleLocalNudge(unbacked_off_types, FROM_HERE);
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());

  StopSyncScheduler();
}

// Test nudges / polls don't run in config mode and config tasks do.
TEST_F(SyncSchedulerImplTest, ConfigurationMode) {
  TimeDelta poll(TimeDelta::FromMilliseconds(15));
  SyncShareTimes times;
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  StartSyncConfiguration();

  const ModelTypeSet nudge_types(TYPED_URLS);
  scheduler()->ScheduleLocalNudge(nudge_types, FROM_HERE);
  scheduler()->ScheduleLocalNudge(nudge_types, FROM_HERE);

  const ModelTypeSet config_types(THEMES);

  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateConfigureSuccess),
                      RecordSyncShare(&times, true)))
      .RetiresOnSaturation();
  CallbackCounter ready_counter;
  ConfigurationParams params(
      sync_pb::SyncEnums::RECONFIGURATION, config_types,
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)));
  scheduler()->ScheduleConfiguration(params);
  RunLoop();
  ASSERT_EQ(1, ready_counter.times_called());

  Mock::VerifyAndClearExpectations(syncer());

  // Switch to NORMAL_MODE to ensure NUDGES were properly saved and run.
  scheduler()->OnReceivedPollIntervalUpdate(TimeDelta::FromDays(1));
  SyncShareTimes times2;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times2, true)));

  StartSyncScheduler(base::Time());

  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
}

class BackoffTriggersSyncSchedulerImplTest : public SyncSchedulerImplTest {
  void SetUp() override {
    SyncSchedulerImplTest::SetUp();
    UseMockDelayProvider();
    EXPECT_CALL(*delay(), GetDelay(_))
        .WillRepeatedly(Return(TimeDelta::FromMilliseconds(10)));
  }

  void TearDown() override {
    StopSyncScheduler();
    SyncSchedulerImplTest::TearDown();
  }
};

// Have the syncer fail during commit.  Expect that the scheduler enters
// backoff.
TEST_F(BackoffTriggersSyncSchedulerImplTest, FailCommitOnce) {
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateCommitFailed),
                      QuitLoopNowAction(false)));
  EXPECT_TRUE(RunAndGetBackoff());
}

// Have the syncer fail during download updates and succeed on the first
// retry.  Expect that this clears the backoff state.
TEST_F(BackoffTriggersSyncSchedulerImplTest, FailDownloadOnceThenSucceed) {
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateDownloadUpdatesFailed),
                      Return(false)))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      QuitLoopNowAction(true)));
  EXPECT_FALSE(RunAndGetBackoff());
}

// Have the syncer fail during commit and succeed on the first retry.  Expect
// that this clears the backoff state.
TEST_F(BackoffTriggersSyncSchedulerImplTest, FailCommitOnceThenSucceed) {
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateCommitFailed), Return(false)))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      QuitLoopNowAction(true)));
  EXPECT_FALSE(RunAndGetBackoff());
}

// Have the syncer fail to download updates and fail again on the retry.
// Expect this will leave the scheduler in backoff.
TEST_F(BackoffTriggersSyncSchedulerImplTest, FailDownloadTwice) {
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateDownloadUpdatesFailed),
                      Return(false)))
      .WillRepeatedly(DoAll(Invoke(test_util::SimulateDownloadUpdatesFailed),
                            QuitLoopNowAction(false)));
  EXPECT_TRUE(RunAndGetBackoff());
}

// Have the syncer fail to get the encryption key yet succeed in downloading
// updates. Expect this will leave the scheduler in backoff.
TEST_F(BackoffTriggersSyncSchedulerImplTest, FailGetEncryptionKey) {
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateGetEncryptionKeyFailed),
                      Return(false)))
      .WillRepeatedly(DoAll(Invoke(test_util::SimulateGetEncryptionKeyFailed),
                            QuitLoopNowAction(false)));
  StartSyncConfiguration();

  ModelTypeSet types(THEMES);
  CallbackCounter ready_counter;
  ConfigurationParams params(
      sync_pb::SyncEnums::RECONFIGURATION, types,
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)));
  scheduler()->ScheduleConfiguration(params);
  RunLoop();

  EXPECT_TRUE(scheduler()->IsGlobalBackoff());
}

// Test that no polls or extraneous nudges occur when in backoff.
TEST_F(SyncSchedulerImplTest, BackoffDropsJobs) {
  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromMilliseconds(10));
  const ModelTypeSet types(THEMES);
  scheduler()->OnReceivedPollIntervalUpdate(poll);
  UseMockDelayProvider();

  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateCommitFailed),
                      RecordSyncShareMultiple(&times, 1U, false)));
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromDays(1)));

  StartSyncScheduler(base::Time());

  // This nudge should fail and put us into backoff.  Thanks to our mock
  // GetDelay() setup above, this will be a long backoff.
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  RunLoop();

  // From this point forward, no SyncShare functions should be invoked.
  Mock::VerifyAndClearExpectations(syncer());

  // Wait a while (10x poll interval) so a few poll jobs will be attempted.
  task_environment_.FastForwardBy(poll * 10);

  // Try (and fail) to schedule a nudge.
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);

  Mock::VerifyAndClearExpectations(syncer());
  Mock::VerifyAndClearExpectations(delay());

  EXPECT_CALL(*delay(), GetDelay(_)).Times(0);

  StartSyncConfiguration();

  CallbackCounter ready_counter;
  ConfigurationParams params(
      sync_pb::SyncEnums::RECONFIGURATION, types,
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)));
  scheduler()->ScheduleConfiguration(params);
  PumpLoop();
  ASSERT_EQ(0, ready_counter.times_called());
}

// Test that backoff is shaping traffic properly with consecutive errors.
TEST_F(SyncSchedulerImplTest, BackoffElevation) {
  SyncShareTimes times;
  UseMockDelayProvider();

  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .Times(kMinNumSamples)
      .WillRepeatedly(
          DoAll(Invoke(test_util::SimulateCommitFailed),
                RecordSyncShareMultiple(&times, kMinNumSamples, false)));

  const TimeDelta first = TimeDelta::FromSeconds(kInitialBackoffRetrySeconds);
  const TimeDelta second = TimeDelta::FromMilliseconds(20);
  const TimeDelta third = TimeDelta::FromMilliseconds(30);
  const TimeDelta fourth = TimeDelta::FromMilliseconds(40);
  const TimeDelta fifth = TimeDelta::FromMilliseconds(50);
  const TimeDelta sixth = TimeDelta::FromDays(1);

  EXPECT_CALL(*delay(), GetDelay(first))
      .WillOnce(Return(second))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(second))
      .WillOnce(Return(third))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(third))
      .WillOnce(Return(fourth))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(fourth))
      .WillOnce(Return(fifth))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(fifth)).WillOnce(Return(sixth));

  StartSyncScheduler(base::Time());

  // Run again with a nudge.
  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  RunLoop();

  ASSERT_EQ(kMinNumSamples, times.size());
  EXPECT_GE(times[1] - times[0], second);
  EXPECT_GE(times[2] - times[1], third);
  EXPECT_GE(times[3] - times[2], fourth);
  EXPECT_GE(times[4] - times[3], fifth);
}

// Test that things go back to normal once a retry makes forward progress.
TEST_F(SyncSchedulerImplTest, BackoffRelief) {
  SyncShareTimes times;
  UseMockDelayProvider();

  const TimeDelta backoff = TimeDelta::FromMilliseconds(10);
  EXPECT_CALL(*delay(), GetDelay(_)).WillOnce(Return(backoff));

  // Optimal start for the post-backoff poll party.
  TimeTicks optimal_start = TimeTicks::Now();
  StartSyncScheduler(base::Time());

  // Kick off the test with a failed nudge.
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateCommitFailed),
                      RecordSyncShare(&times, false)));
  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
  TimeTicks optimal_job_time = optimal_start;
  ASSERT_EQ(1U, times.size());
  EXPECT_GE(times[0], optimal_job_time);

  // The retry succeeds.
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)));
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
  optimal_job_time = optimal_job_time + backoff;
  ASSERT_EQ(2U, times.size());
  EXPECT_GE(times[1], optimal_job_time);

  // Now let the Poll timer do its thing.
  EXPECT_CALL(*syncer(), PollSyncShare(_, _))
      .WillRepeatedly(
          DoAll(Invoke(test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));
  const TimeDelta poll(TimeDelta::FromMilliseconds(10));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  // The new optimal time is now, since the desired poll should have happened
  // in the past.
  optimal_job_time = TimeTicks::Now();
  RunLoop();
  Mock::VerifyAndClearExpectations(syncer());
  ASSERT_EQ(kMinNumSamples, times.size());
  for (size_t i = 2; i < times.size(); i++) {
    SCOPED_TRACE(testing::Message() << "SyncShare # (" << i << ")");
    EXPECT_GE(times[i], optimal_job_time);
    optimal_job_time = optimal_job_time + poll;
  }

  StopSyncScheduler();
}

// Test that poll failures are treated like any other failure. They should
// result in retry with backoff.
TEST_F(SyncSchedulerImplTest, TransientPollFailure) {
  SyncShareTimes times;
  const TimeDelta poll_interval(TimeDelta::FromMilliseconds(10));
  scheduler()->OnReceivedPollIntervalUpdate(poll_interval);
  UseMockDelayProvider();  // Will cause test failure if backoff is initiated.
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(0)));

  EXPECT_CALL(*syncer(), PollSyncShare(_, _))
      .WillOnce(DoAll(Invoke(test_util::SimulatePollFailed),
                      RecordSyncShare(&times, false)))
      .WillOnce(DoAll(Invoke(test_util::SimulatePollSuccess),
                      RecordSyncShare(&times, true)));

  StartSyncScheduler(base::Time());

  // Run the unsuccessful poll. The failed poll should not trigger backoff.
  RunLoop();
  EXPECT_TRUE(scheduler()->IsGlobalBackoff());

  // Run the successful poll.
  RunLoop();
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
}

// Test that starting the syncer thread without a valid connection doesn't
// break things when a connection is detected.
TEST_F(SyncSchedulerImplTest, StartWhenNotConnected) {
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(
          DoAll(Invoke(test_util::SimulateConnectionFailure), Return(false)))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess), Return(true)));
  StartSyncScheduler(base::Time());

  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  // Should save the nudge for until after the server is reachable.
  base::RunLoop().RunUntilIdle();

  scheduler()->OnConnectionStatusChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  connection()->SetServerReachable();
  connection()->UpdateConnectionStatus();
  base::RunLoop().RunUntilIdle();
}

// Test that when disconnect signal (CONNECTION_NONE) is received, normal sync
// share is not called.
TEST_F(SyncSchedulerImplTest, SyncShareNotCalledWhenDisconnected) {
  // Set server unavailable, so SyncSchedulerImpl will try to fix connection
  // error upon OnConnectionStatusChange().
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .Times(1)
      .WillOnce(
          DoAll(Invoke(test_util::SimulateConnectionFailure), Return(false)));
  StartSyncScheduler(base::Time());

  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  // The nudge fails because of the connection failure.
  base::RunLoop().RunUntilIdle();

  // Simulate a disconnect signal. The scheduler should not retry the previously
  // failed nudge.
  scheduler()->OnConnectionStatusChange(
      network::mojom::ConnectionType::CONNECTION_NONE);
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncSchedulerImplTest, ServerConnectionChangeDuringBackoff) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(0)));

  StartSyncScheduler(base::Time());
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();

  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(
          DoAll(Invoke(test_util::SimulateConnectionFailure), Return(false)))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess), Return(true)));

  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // Run the nudge, that will fail and schedule a quick retry.
  ASSERT_TRUE(scheduler()->IsGlobalBackoff());

  // Before we run the scheduled canary, trigger a server connection change.
  scheduler()->OnConnectionStatusChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  connection()->SetServerReachable();
  connection()->UpdateConnectionStatus();
  base::RunLoop().RunUntilIdle();
}

// This was supposed to test the scenario where we receive a nudge while a
// connection change canary is scheduled, but has not run yet.  Since we've made
// the connection change canary synchronous, this is no longer possible.
TEST_F(SyncSchedulerImplTest, ConnectionChangeCanaryPreemptedByNudge) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(0)));

  StartSyncScheduler(base::Time());
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();

  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(
          DoAll(Invoke(test_util::SimulateConnectionFailure), Return(false)))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess), Return(true)))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      QuitLoopNowAction(true)));

  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);

  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // Run the nudge, that will fail and schedule a quick retry.
  ASSERT_TRUE(scheduler()->IsGlobalBackoff());

  // Before we run the scheduled canary, trigger a server connection change.
  scheduler()->OnConnectionStatusChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  PumpLoop();
  connection()->SetServerReachable();
  connection()->UpdateConnectionStatus();
  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  base::RunLoop().RunUntilIdle();
}

// Tests that we don't crash trying to run two canaries at once if we receive
// extra connection status change notifications.  See crbug.com/190085.
TEST_F(SyncSchedulerImplTest, DoubleCanaryInConfigure) {
  EXPECT_CALL(*syncer(), ConfigureSyncShare(_, _, _))
      .WillRepeatedly(DoAll(
          Invoke(test_util::SimulateConfigureConnectionFailure), Return(true)));
  StartSyncConfiguration();
  connection()->SetServerNotReachable();
  connection()->UpdateConnectionStatus();

  ModelTypeSet model_types(THEMES);
  CallbackCounter ready_counter;
  ConfigurationParams params(
      sync_pb::SyncEnums::RECONFIGURATION, model_types,
      base::Bind(&CallbackCounter::Callback, base::Unretained(&ready_counter)));
  scheduler()->ScheduleConfiguration(params);

  scheduler()->OnConnectionStatusChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  scheduler()->OnConnectionStatusChange(
      network::mojom::ConnectionType::CONNECTION_WIFI);

  PumpLoop();  // Run the nudge, that will fail and schedule a quick retry.
}

TEST_F(SyncSchedulerImplTest, PollFromCanaryAfterAuthError) {
  SyncShareTimes times;
  TimeDelta poll(TimeDelta::FromMilliseconds(15));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), PollSyncShare(_, _))
      .WillRepeatedly(
          DoAll(Invoke(test_util::SimulatePollSuccess),
                RecordSyncShareMultiple(&times, kMinNumSamples, true)));

  connection()->SetServerResponse(
      HttpResponse::ForHttpError(net::HTTP_UNAUTHORIZED));
  StartSyncScheduler(base::Time());

  // Run to wait for polling.
  RunLoop();

  // Normally OnCredentialsUpdated calls TryCanaryJob that doesn't run Poll,
  // but after poll finished with auth error from poll timer it should retry
  // poll once more
  EXPECT_CALL(*syncer(), PollSyncShare(_, _))
      .WillOnce(DoAll(Invoke(test_util::SimulatePollSuccess),
                      RecordSyncShare(&times, true)));
  scheduler()->OnCredentialsUpdated();
  connection()->SetServerResponse(HttpResponse::ForSuccess());
  RunLoop();
  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, SuccessfulRetry) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times;
  TimeDelta delay = TimeDelta::FromMilliseconds(10);
  scheduler()->OnReceivedGuRetryDelay(delay);
  EXPECT_EQ(delay, GetRetryTimerDelay());

  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)));

  // Run to wait for retrying.
  RunLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, FailedRetry) {
  SyncShareTimes times;

  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillRepeatedly(Return(TimeDelta::FromMilliseconds(10)));

  StartSyncScheduler(base::Time());

  TimeDelta delay = TimeDelta::FromMilliseconds(10);
  scheduler()->OnReceivedGuRetryDelay(delay);

  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateDownloadUpdatesFailed),
                      RecordSyncShare(&times, false)));

  // Run to wait for retrying.
  RunLoop();

  EXPECT_TRUE(scheduler()->IsGlobalBackoff());
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)));

  // Run to wait for second retrying.
  RunLoop();

  StopSyncScheduler();
}

ACTION_P2(VerifyRetryTimerDelay, scheduler_test, expected_delay) {
  EXPECT_EQ(expected_delay, scheduler_test->GetRetryTimerDelay());
}

TEST_F(SyncSchedulerImplTest, ReceiveNewRetryDelay) {
  StartSyncScheduler(base::Time());

  SyncShareTimes times;
  TimeDelta delay1 = TimeDelta::FromMilliseconds(100);
  TimeDelta delay2 = TimeDelta::FromMilliseconds(200);

  scheduler()->ScheduleLocalNudge(ModelTypeSet(THEMES), FROM_HERE);
  scheduler()->OnReceivedGuRetryDelay(delay1);
  EXPECT_EQ(delay1, GetRetryTimerDelay());

  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(
          DoAll(WithoutArgs(VerifyRetryTimerDelay(this, delay1)),
                WithArg<2>(test_util::SimulateGuRetryDelayCommand(delay2)),
                RecordSyncShare(&times, true)));

  // Run nudge GU.
  RunLoop();
  EXPECT_EQ(delay2, GetRetryTimerDelay());

  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)));

  // Run to wait for retrying.
  RunLoop();

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, PartialFailureWillExponentialBackoff) {
  TimeDelta poll(TimeDelta::FromDays(1));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelTypeSet types(THEMES);

  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillRepeatedly(DoAll(
          WithArg<2>(test_util::SimulatePartialFailure(types)), Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().HasAll(types));
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());
  TimeDelta first_blocking_time = GetTypeBlockingTime(THEMES);

  SetTypeBlockingMode(THEMES, WaitInterval::EXPONENTIAL_BACKOFF_RETRYING);
  // This won't cause a sync cycle because the types are backed off.
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop();
  PumpLoop();
  TimeDelta second_blocking_time = GetTypeBlockingTime(THEMES);

  // The Exponential backoff should be between previous backoff 1.5 and 2.5
  // times.
  EXPECT_LE(first_blocking_time * 1.5, second_blocking_time);
  EXPECT_GE(first_blocking_time * 2.5, second_blocking_time);

  StopSyncScheduler();
}

// If a datatype is in backoff or throttling, pending_wakeup_timer_ should
// schedule a delay job for OnTypesUnblocked. SyncScheduler sometimes use
// pending_wakeup_timer_ to schdule PerformDelayedNudge job before
// OnTypesUnblocked got run. This test will verify after ran
// PerformDelayedNudge, OnTypesUnblocked will be rescheduled if any datatype is
// in backoff or throttling.
TEST_F(SyncSchedulerImplTest, TypeBackoffAndSuccessfulSync) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_)).WillRepeatedly(Return(long_delay()));

  TimeDelta poll(TimeDelta::FromDays(1));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  const ModelTypeSet types(THEMES);

  // Set backoff datatype.
  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(WithArg<2>(test_util::SimulatePartialFailure(types)),
                      Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(types, FROM_HERE);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().HasAll(types));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateNormalSuccess),
                      RecordSyncShare(&times, true)))
      .RetiresOnSaturation();

  // Do a successful Sync.
  const ModelTypeSet unbacked_off_types(TYPED_URLS);
  scheduler()->ScheduleLocalNudge(unbacked_off_types, FROM_HERE);
  PumpLoop();  // TO get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called.

  // Timer is still running for backoff datatype after Sync success.
  EXPECT_TRUE(GetBackedOffTypes().HasAll(types));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  StopSyncScheduler();
}

// Verify that the timer is scheduled for an unblock job after one datatype is
// unblocked, and there is another one still blocked.
TEST_F(SyncSchedulerImplTest, TypeBackingOffAndFailureSync) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillOnce(Return(long_delay()))
      .RetiresOnSaturation();

  TimeDelta poll(TimeDelta::FromDays(1));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  // Set a backoff datatype.
  const ModelTypeSet themes_types(THEMES);
  ::testing::InSequence seq;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(
          DoAll(WithArg<2>(test_util::SimulatePartialFailure(themes_types)),
                Return(true)))
      .RetiresOnSaturation();

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge(themes_types, FROM_HERE);
  PumpLoop();  // To get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called
  EXPECT_TRUE(GetBackedOffTypes().HasAll(themes_types));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // Set anther backoff datatype.
  const ModelTypeSet typed_urls_types(TYPED_URLS);
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(
          DoAll(WithArg<2>(test_util::SimulatePartialFailure(typed_urls_types)),
                Return(true)))
      .RetiresOnSaturation();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillOnce(Return(default_delay()))
      .RetiresOnSaturation();

  scheduler()->ScheduleLocalNudge(typed_urls_types, FROM_HERE);
  PumpLoop();  // TO get PerformDelayedNudge called.
  PumpLoop();  // To get TrySyncCycleJob called.

  EXPECT_TRUE(GetBackedOffTypes().HasAll(themes_types));
  EXPECT_TRUE(GetBackedOffTypes().HasAll(typed_urls_types));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  // Unblock one datatype.
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillRepeatedly(DoAll(Invoke(test_util::SimulateNormalSuccess),
                            RecordSyncShare(&times, true)));
  EXPECT_CALL(*delay(), GetDelay(_)).WillRepeatedly(Return(long_delay()));

  PumpLoop();  // TO get OnTypesUnblocked called.
  PumpLoop();  // To get TrySyncCycleJob called.

  // Timer is still scheduled for another backoff datatype.
  EXPECT_TRUE(GetBackedOffTypes().HasAll(themes_types));
  EXPECT_FALSE(GetBackedOffTypes().HasAll(typed_urls_types));
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());
  EXPECT_FALSE(scheduler()->IsGlobalThrottle());

  StopSyncScheduler();
}

TEST_F(SyncSchedulerImplTest, InterleavedNudgesStillRestart) {
  UseMockDelayProvider();
  EXPECT_CALL(*delay(), GetDelay(_))
      .WillOnce(Return(long_delay()))
      .RetiresOnSaturation();
  TimeDelta poll(TimeDelta::FromDays(1));
  scheduler()->OnReceivedPollIntervalUpdate(poll);

  StartSyncScheduler(base::Time());
  scheduler()->ScheduleLocalNudge({THEMES}, FROM_HERE);
  PumpLoop();  // To get PerformDelayedNudge called.
  EXPECT_FALSE(BlockTimerIsRunning());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());

  // This is the tricky piece. We have a gap while the sync job is bouncing to
  // get onto the |pending_wakeup_timer_|, should be scheduled with no delay.
  scheduler()->ScheduleLocalNudge({TYPED_URLS}, FROM_HERE);
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_EQ(TimeDelta(), GetPendingWakeupTimerDelay());
  EXPECT_FALSE(scheduler()->IsGlobalBackoff());

  // Setup mock as we're about to attempt to sync.
  SyncShareTimes times;
  EXPECT_CALL(*syncer(), NormalSyncShare(_, _, _))
      .WillOnce(DoAll(Invoke(test_util::SimulateCommitFailed),
                      RecordSyncShare(&times, false)));
  // Triggers the THEMES TrySyncCycleJobImpl(), which we've setup to fail. Its
  // RestartWaiting won't schedule a delayed retry, as the TYPED_URLS nudge has
  // a smaller delay. We verify this by making sure the delay is still zero.
  PumpLoop();
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_EQ(TimeDelta(), GetPendingWakeupTimerDelay());
  EXPECT_TRUE(scheduler()->IsGlobalBackoff());

  // Triggers TYPED_URLS PerformDelayedNudge(), which should no-op, because
  // we're no long healthy, and normal priorities shouldn't go through, but it
  // does need to setup the |pending_wakeup_timer_|. The delay should be ~60
  // seconds, so verifying it's greater than 50 should be safe.
  PumpLoop();
  EXPECT_TRUE(BlockTimerIsRunning());
  EXPECT_LT(TimeDelta::FromSeconds(50), GetPendingWakeupTimerDelay());
  EXPECT_TRUE(scheduler()->IsGlobalBackoff());
}

TEST_F(SyncSchedulerImplTest, PollOnStartUpAfterLongPause) {
  base::Time now = base::Time::Now();
  base::TimeDelta poll_interval = base::TimeDelta::FromHours(4);
  base::Time last_reset = ComputeLastPollOnStart(
      /*last_poll=*/now - base::TimeDelta::FromDays(1), poll_interval, now);
  EXPECT_THAT(last_reset, Gt(now - poll_interval));
  // The max poll delay is 1% of the poll_interval.
  EXPECT_THAT(last_reset, Lt(now - 0.99 * poll_interval));
}

TEST_F(SyncSchedulerImplTest, PollOnStartUpAfterShortPause) {
  base::Time now = base::Time::Now();
  base::TimeDelta poll_interval = base::TimeDelta::FromHours(4);
  base::Time last_poll = now - base::TimeDelta::FromHours(2);
  EXPECT_THAT(ComputeLastPollOnStart(last_poll, poll_interval, now),
              Eq(last_poll));
}

// Verifies that the delay is in [0, 0.01*poll_interval) and spot checks the
// random number generation.
TEST_F(SyncSchedulerImplTest, PollOnStartUpWithinBoundsAfterLongPause) {
  base::Time now = base::Time::Now();
  base::TimeDelta poll_interval = base::TimeDelta::FromHours(4);
  base::Time last_poll = now - base::TimeDelta::FromDays(2);
  bool found_delay_greater_than_5_permille = false;
  bool found_delay_less_or_equal_5_permille = false;
  for (int i = 0; i < 10000; ++i) {
    base::Time result = ComputeLastPollOnStart(last_poll, poll_interval, now);
    base::TimeDelta delay = result + poll_interval - now;
    double fraction = delay.InSeconds() * 1.0 / poll_interval.InSeconds();
    if (fraction > 0.005) {
      found_delay_greater_than_5_permille = true;
    } else {
      found_delay_less_or_equal_5_permille = true;
    }
    EXPECT_THAT(fraction, Ge(0));
    EXPECT_THAT(fraction, Lt(0.01));
  }
  EXPECT_TRUE(found_delay_greater_than_5_permille);
  EXPECT_TRUE(found_delay_less_or_equal_5_permille);
}

TEST_F(SyncSchedulerImplTest, TestResetPollIntervalOnStartFeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(switches::kSyncResetPollIntervalOnStart);
  base::Time now = base::Time::Now();
  base::TimeDelta poll_interval = base::TimeDelta::FromHours(4);
  EXPECT_THAT(
      ComputeLastPollOnStart(
          /*last_poll=*/now - base::TimeDelta::FromDays(1), poll_interval, now),
      Eq(now));
}

}  // namespace syncer
