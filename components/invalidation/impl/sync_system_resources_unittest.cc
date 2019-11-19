// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/sync_system_resources.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/invalidation/impl/push_client_channel.h"
#include "components/invalidation/impl/state_writer.h"
#include "google/cacheinvalidation/include/types.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using ::testing::_;
using ::testing::SaveArg;

class MockStateWriter : public StateWriter {
 public:
  MOCK_METHOD1(WriteState, void(const std::string&));
};

class MockClosure {
 public:
  MOCK_CONST_METHOD0(Run, void(void));
  base::Closure* CreateClosure() {
    return new base::Closure(
        base::Bind(&MockClosure::Run, base::Unretained(this)));
  }
};

class MockStorageCallback {
 public:
  MOCK_CONST_METHOD1(Run, void(invalidation::Status));
  base::Callback<void(invalidation::Status)>* CreateCallback() {
    return new base::Callback<void(invalidation::Status)>(
        base::Bind(&MockStorageCallback::Run, base::Unretained(this)));
  }
};

class SyncSystemResourcesTest : public testing::Test {
 protected:
  SyncSystemResourcesTest()
      : push_client_channel_(std::make_unique<notifier::FakePushClient>()),
        sync_system_resources_(&push_client_channel_, &mock_state_writer_) {}

  ~SyncSystemResourcesTest() override {}

  void ScheduleShouldNotRun() {
    {
      // Owned by ScheduleImmediately.
      MockClosure mock_closure;
      base::Closure* should_not_run = mock_closure.CreateClosure();
      EXPECT_CALL(mock_closure, Run()).Times(0);
      sync_system_resources_.internal_scheduler()->Schedule(
          invalidation::Scheduler::NoDelay(), should_not_run);
    }
    {
      // Owned by ScheduleOnListenerThread.
      MockClosure mock_closure;
      base::Closure* should_not_run = mock_closure.CreateClosure();
      EXPECT_CALL(mock_closure, Run()).Times(0);
      sync_system_resources_.listener_scheduler()->Schedule(
          invalidation::Scheduler::NoDelay(), should_not_run);
    }
    {
      // Owned by ScheduleWithDelay.
      MockClosure mock_closure;
      base::Closure* should_not_run = mock_closure.CreateClosure();
      EXPECT_CALL(mock_closure, Run()).Times(0);
      sync_system_resources_.internal_scheduler()->Schedule(
          invalidation::TimeDelta::FromSeconds(0), should_not_run);
    }
  }

  // Needed by |sync_system_resources_|.
  base::test::SingleThreadTaskEnvironment task_environment_;
  MockStateWriter mock_state_writer_;
  PushClientChannel push_client_channel_;
  SyncSystemResources sync_system_resources_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncSystemResourcesTest);
};

// Make sure current_time() doesn't crash or leak.
TEST_F(SyncSystemResourcesTest, CurrentTime) {
  invalidation::Time current_time =
      sync_system_resources_.internal_scheduler()->GetCurrentTime();
  DVLOG(1) << "current_time returned: " << current_time.ToInternalValue();
}

// Make sure Log() doesn't crash or leak.
TEST_F(SyncSystemResourcesTest, Log) {
  sync_system_resources_.logger()->Log(SyncLogger::INFO_LEVEL,
                                         __FILE__, __LINE__, "%s %d",
                                         "test string", 5);
}

TEST_F(SyncSystemResourcesTest, ScheduleBeforeStart) {
  ScheduleShouldNotRun();
  sync_system_resources_.Start();
}

TEST_F(SyncSystemResourcesTest, ScheduleAfterStop) {
  sync_system_resources_.Start();
  sync_system_resources_.Stop();
  ScheduleShouldNotRun();
}

TEST_F(SyncSystemResourcesTest, ScheduleAndStop) {
  sync_system_resources_.Start();
  ScheduleShouldNotRun();
  sync_system_resources_.Stop();
}

TEST_F(SyncSystemResourcesTest, ScheduleAndDestroy) {
  sync_system_resources_.Start();
  ScheduleShouldNotRun();
}

TEST_F(SyncSystemResourcesTest, ScheduleImmediately) {
  sync_system_resources_.Start();
  MockClosure mock_closure;
  EXPECT_CALL(mock_closure, Run());
  sync_system_resources_.internal_scheduler()->Schedule(
      invalidation::Scheduler::NoDelay(), mock_closure.CreateClosure());
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncSystemResourcesTest, ScheduleOnListenerThread) {
  sync_system_resources_.Start();
  MockClosure mock_closure;
  EXPECT_CALL(mock_closure, Run());
  sync_system_resources_.listener_scheduler()->Schedule(
      invalidation::Scheduler::NoDelay(), mock_closure.CreateClosure());
  EXPECT_TRUE(
      sync_system_resources_.internal_scheduler()->IsRunningOnThread());
  base::RunLoop().RunUntilIdle();
}

TEST_F(SyncSystemResourcesTest, ScheduleWithZeroDelay) {
  sync_system_resources_.Start();
  MockClosure mock_closure;
  EXPECT_CALL(mock_closure, Run());
  sync_system_resources_.internal_scheduler()->Schedule(
      invalidation::TimeDelta::FromSeconds(0), mock_closure.CreateClosure());
  base::RunLoop().RunUntilIdle();
}

// TODO(akalin): Figure out how to test with a non-zero delay.

TEST_F(SyncSystemResourcesTest, WriteState) {
  sync_system_resources_.Start();
  EXPECT_CALL(mock_state_writer_, WriteState(_));
  // Owned by WriteState.
  MockStorageCallback mock_storage_callback;
  invalidation::Status results(invalidation::Status::PERMANENT_FAILURE,
                               "fake-failure");
  EXPECT_CALL(mock_storage_callback, Run(_))
      .WillOnce(SaveArg<0>(&results));
  sync_system_resources_.storage()->WriteKey(
      std::string(), "state", mock_storage_callback.CreateCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation::Status(invalidation::Status::SUCCESS, std::string()),
            results);
}

class TestSyncNetworkChannel : public SyncNetworkChannel {
 public:
  TestSyncNetworkChannel() {}
  ~TestSyncNetworkChannel() override {}

  using SyncNetworkChannel::NotifyNetworkStatusChange;
  using SyncNetworkChannel::NotifyChannelStateChange;
  using SyncNetworkChannel::DeliverIncomingMessage;

  void SendMessage(const std::string& message) override {}

  void UpdateCredentials(const CoreAccountId& account_id,
                         const std::string& token) override {}

  int GetInvalidationClientType() override { return 0; }

  void RequestDetailedStatus(
      base::Callback<void(const base::DictionaryValue&)> callback) override {
    base::DictionaryValue value;
    callback.Run(value);
  }
};

class SyncNetworkChannelTest
    : public testing::Test,
      public SyncNetworkChannel::Observer {
 protected:
  SyncNetworkChannelTest()
      : last_invalidator_state_(DEFAULT_INVALIDATION_ERROR),
        connected_(false) {
    network_channel_.AddObserver(this);
    network_channel_.AddNetworkStatusReceiver(
        invalidation::NewPermanentCallback(
            this, &SyncNetworkChannelTest::OnNetworkStatusChange));
  }

  ~SyncNetworkChannelTest() override { network_channel_.RemoveObserver(this); }

  void OnNetworkChannelStateChanged(
      InvalidatorState invalidator_state) override {
    last_invalidator_state_ = invalidator_state;
  }

  void OnNetworkStatusChange(bool connected) {
    connected_ = connected;
  }

  TestSyncNetworkChannel network_channel_;
  InvalidatorState last_invalidator_state_;
  bool connected_;
};

// Simulate channel state change. It should propagate to observer.
TEST_F(SyncNetworkChannelTest, ChannelStateChange) {
  EXPECT_EQ(DEFAULT_INVALIDATION_ERROR, last_invalidator_state_);
  network_channel_.NotifyChannelStateChange(INVALIDATIONS_ENABLED);
  EXPECT_EQ(INVALIDATIONS_ENABLED, last_invalidator_state_);
  network_channel_.NotifyChannelStateChange(INVALIDATION_CREDENTIALS_REJECTED);
  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, last_invalidator_state_);
}

// Simulate network state change. It should propagate to cacheinvalidations.
TEST_F(SyncNetworkChannelTest, NetworkStateChange) {
  EXPECT_FALSE(connected_);
  network_channel_.NotifyNetworkStatusChange(true);
  EXPECT_TRUE(connected_);
  network_channel_.NotifyNetworkStatusChange(false);
  EXPECT_FALSE(connected_);
}

}  // namespace
}  // namespace syncer
