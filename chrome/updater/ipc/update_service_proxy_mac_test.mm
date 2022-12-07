// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_proxy_mac.h"

#import <Foundation/Foundation.h>

#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/mac/scoped_block.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_policy.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#import "chrome/updater/app/server/mac/service_protocol.h"
#import "chrome/updater/app/server/mac/update_service_wrappers.h"
#include "chrome/updater/mac/scoped_xpc_service_mock.h"
#import "chrome/updater/mac/xpc_service_names.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/unittest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

namespace updater {

// Simulates a stream of updater events over the interfaces used for XPC,
// and verifies that we receive them, translated, in the corresponding C++
// callback interface.
//
// This requires QUEUED mode in a SingleThreadTaskEnvironment.
// TestTimeouts::Initialize() must have been called before use.
class StateChangeTestEngine {
 public:
  // StatePair represents an event that will be provided in Objective-C XPC
  // format to an id<CRUUpdateStateObserving> as part of a sequence of partial
  // progress callbacks, and the equivalent UpdateState it should be decoded to.
  using StatePair = std::pair<base::scoped_nsobject<CRUUpdateStateWrapper>,
                              UpdateService::UpdateState>;

  // Construct a StateChangeTestEngine that will issue the given states in
  // the given sequence, and verify that it observes the expected items
  // in its callback.
  explicit StateChangeTestEngine(std::vector<StatePair> state_vec);

  ~StateChangeTestEngine();

  // Create a StateChangeCallback that will verify that we see the expected
  // UpdateState objects in the expected order. Must be called exactly once,
  // and must be called before |this| begins simulating events.
  UpdateService::StateChangeCallback Watch();

  // Start injecting events. Must be called exactly once and must be called
  // on the main sequence. Must be called after watch().
  //
  // observer is the "event sink", which should be the object expecting to
  // receive CRUUpdateState objects across an XPC connection. done_cb is
  // invoked when the test completes or times out, regardless of the outcome
  // of the test - this is intended to be used to send the final reply.
  void StartSimulating(
      base::scoped_nsprotocol<id<CRUUpdateStateObserving>> observer,
      base::OnceClosure done_cb);

 private:
  using vec_size_t = std::vector<StatePair>::size_type;
  // Sentinel for "we have not started sending events yet".
  static constexpr vec_size_t kNotStarted =
      std::numeric_limits<vec_size_t>::max();
  // Sentinel for "we don't expect to observe anything because we are done".
  static constexpr vec_size_t kDone = kNotStarted - 1;

  // Implementation of the callback created by Watch().
  void GotState(const UpdateService::UpdateState& state);

  // Push the next event (if any).
  void Next();

  // Assert that we aren't already done, mark ourselves done, call the
  // finish callback.
  void Finish();

  // Fail and halt if the test has not progressed, using the index of the last
  // pushed event to identify progression.
  void MaybeTimeOut(vec_size_t last_pushed_event);

  // Iterate through state_vec, trying to find the first expected state that
  // the provided state *is* identical to. If it isn't identical to any of
  // them, try again with a weaker check - what is it *similar* to?
  // Log a test failure message with our discovery.
  // If it's not similar to any of them, log a test failure message saying so
  // and dumping the UpdateState itself.
  void SearchForBadState(const UpdateService::UpdateState& state);

  std::vector<StatePair> state_seq_;
  // Index of the UpdateState we expect to see next. Also calculates which
  // CRUUpdateStateWrapper we issue next.
  vec_size_t next_observation_ = kNotStarted;
  // Whether Watch() has yet been called to create a callback for this engine.
  bool callback_prepared_ = false;
  // Object to send Objective-C update state events to.
  base::scoped_nsprotocol<id<CRUUpdateStateObserving>> observer_;
  // Invoke when we have no more callbacks to invoke. Only |Finish()| is allowed
  // to call this.
  base::OnceClosure done_cb_;
};  // class StateChangeTestEngine

// C++ requires these to be explicitly defined outside the class declarator
// in C++11 and below; this can be skipped in C++17.
const StateChangeTestEngine::vec_size_t StateChangeTestEngine::kNotStarted;
const StateChangeTestEngine::vec_size_t StateChangeTestEngine::kDone;

StateChangeTestEngine::StateChangeTestEngine(std::vector<StatePair> state_vec)
    : state_seq_(std::move(state_vec)) {}

StateChangeTestEngine::~StateChangeTestEngine() {
  EXPECT_NE(next_observation_, kNotStarted)
      << "TEST ISSUE: StateChangeTestEngine never started";
  EXPECT_EQ(next_observation_, kDone)
      << "StateChangeTestEngine cleanup: not done! waiting for reply: "
      << next_observation_;
  EXPECT_TRUE(done_cb_.is_null())
      << "StateChangeTestEngine cleanup: never invoked when_done_ callback";
}

void StateChangeTestEngine::StartSimulating(
    base::scoped_nsprotocol<id<CRUUpdateStateObserving>> observer,
    base::OnceClosure done_cb) {
  EXPECT_TRUE(callback_prepared_)
      << "TEST ISSUE:  StateChangetestEngine cannot StartSimulating without "
         "Watch()ing for event callbacks";
  EXPECT_EQ(next_observation_, kNotStarted)
      << "TEST ISSUE: StateChangeTestEngine already started; not reusable";
  EXPECT_GT(state_seq_.size(), 0UL)
      << "TEST ISSUE: StateChangeTestEngine unwilling to simulate 0 events - "
         "use ExpectNoCalls instead if this is intentional";

  observer_ = observer;
  done_cb_ = std::move(done_cb);

  Next();
}

void StateChangeTestEngine::Next() {
  ASSERT_NE(next_observation_, kDone)
      << "TEST ISSUE: StateChangeTestEngine trying to increment after it's "
         "already done. This is a StateChangeTestEngine bug.";

  if (next_observation_ == kNotStarted) {
    next_observation_ = 0;
  } else {
    ++next_observation_;
  }

  if (next_observation_ >= state_seq_.size()) {
    Finish();
    return;
  }
  // Asynchronously issue the update state change.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::scoped_nsprotocol<id<CRUUpdateStateObserving>> observer,
             CRUUpdateStateWrapper* state) {
            [observer observeUpdateState:state];
          },
          observer_, state_seq_[next_observation_].first.get()));
  // Prepare to time out.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&StateChangeTestEngine::MaybeTimeOut,
                     base::Unretained(this), next_observation_),
      TestTimeouts::tiny_timeout());
}

void StateChangeTestEngine::MaybeTimeOut(vec_size_t last_pushed_event_) {
  if (last_pushed_event_ == next_observation_) {
    // We have not yet observed the event that created this timeout check.
    // Therefore, it has timed out. Finish, so tests don't just hang.
    ADD_FAILURE() << "StateChangeTestEngine timed out, waiting for event "
                  << last_pushed_event_;
    Finish();
  }
  // Else: take no action. We made progress.
}

void StateChangeTestEngine::Finish() {
  if (next_observation_ == kDone) {
    // One of our invariants has broken down.
    ADD_FAILURE()
        << "TEST ISSUE: StateChangeTestEngine trying to finish() twice. This "
           "is probably a bug in StateChangeTestEngine.";
  }

  next_observation_ = kDone;

  ASSERT_FALSE(done_cb_.is_null())
      << "TEST ISSUE: StateChangeTestEngine doesn't have a valid done_cb_ when "
         "Finish()ing.";
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           std::move(done_cb_));
}

void StateChangeTestEngine::GotState(const UpdateService::UpdateState& state) {
  if (next_observation_ == kDone) {
    ADD_FAILURE() << "StateChangeTestEngine received StateChangeCallback "
                     "event after it became done.";
    SearchForBadState(state);
    return;
  }
  ASSERT_NE(next_observation_, kNotStarted)
      << "TEST ISSUE: StateChangeTestEngine received StateChangeCallback event "
         "before it started simulating any events.";
  ASSERT_LT(next_observation_, state_seq_.size())
      << "TEST ISSUE: StateChangeTestEngine expectation past end of sequence";

  const UpdateService::UpdateState& expected =
      state_seq_[next_observation_].second;
  if (state != expected) {
    ADD_FAILURE() << "StateChangeTestEngine: unexpected UpdateState when "
                     "expecting state at index "
                  << next_observation_;
    SearchForBadState(state);
    // A mismatch does not prevent us from being able to continue the test,
    // so proceed after logging this failure.
  }
  Next();
}

void StateChangeTestEngine::SearchForBadState(
    const UpdateService::UpdateState& state) {
  // First, look for an exact match somewhere in our list.
  for (vec_size_t i = 0; i < state_seq_.size(); ++i) {
    if (state_seq_[i].second == state) {
      LOG(ERROR) << "\tReceived state identical to expected state " << i;
      return;  // Do not emit the received state - we already have a copy.
    }
  }
  LOG(ERROR) << "\tReceived state different from each expected state ";

  // No exact match. The state might be a hint about where to look.
  for (vec_size_t i = 0; i < state_seq_.size(); ++i) {
    if (state_seq_[i].second.state == state.state) {
      LOG(ERROR) << "\tSame event as expected state " << i;
      break;
    }
  }
  // Now emit whatever we got, since it wasn't identical to something we knew
  // of.
  LOG(ERROR) << "Unexpected UpdateState: " << state;
}

UpdateService::StateChangeCallback StateChangeTestEngine::Watch() {
  EXPECT_FALSE(callback_prepared_)
      << "TEST ISSUE: StateChangeTestEngine already watch()ing for events, "
         "watch()ing twice leads to Undesirable Behavior";
  callback_prepared_ = true;
  return base::BindRepeating(&StateChangeTestEngine::GotState,
                             base::Unretained(this));
}

#pragma mark Test fixture

class MacUpdateServiceProxyTest : public ::testing::Test {
 protected:
  void SetUp() override;

  // Create an UpdateServiceProxy and store in service_. Must be
  // called only on the task_environment_ sequence. SetUp() posts it.
  void InitializeUpdateService();

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  ScopedXPCServiceMock mock_driver_ { @protocol (CRUUpdateServicing) };
  std::unique_ptr<base::RunLoop> run_loop_;
  scoped_refptr<UpdateServiceProxy> service_;
};  // class MacUpdateOutOfProcessTest

void MacUpdateServiceProxyTest::SetUp() {
  run_loop_ = std::make_unique<base::RunLoop>();
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this]() {
        service_ = base::MakeRefCounted<UpdateServiceProxy>(
            UpdaterScope::kUser, base::TimeDelta::Max());
      }));
}

base::scoped_nsobject<CRUUpdateStateStateWrapper> ObjCState(
    UpdateService::UpdateState::State state) {
  return base::scoped_nsobject<CRUUpdateStateStateWrapper>(
      [[CRUUpdateStateStateWrapper alloc] initWithUpdateStateState:state]);
}

base::scoped_nsobject<CRUErrorCategoryWrapper> ObjCErrorCategory(
    UpdateService::ErrorCategory category) {
  return base::scoped_nsobject<CRUErrorCategoryWrapper>(
      [[CRUErrorCategoryWrapper alloc] initWithErrorCategory:category]);
}

#pragma mark StatePair helpers

StateChangeTestEngine::StatePair CheckingForUpdatesStates(
    const std::string& app_id) {
  StateChangeTestEngine::StatePair ret;

  UpdateService::UpdateState& update_state = ret.second;
  update_state.app_id = app_id;
  update_state.state = UpdateService::UpdateState::State::kCheckingForUpdates;

  ret.first.reset([[CRUUpdateStateWrapper alloc]
        initWithAppId:base::SysUTF8ToNSString(app_id)
                state:ObjCState(UpdateService::UpdateState::State::
                                    kCheckingForUpdates)
                          .get()
              version:[NSString string]
      downloadedBytes:-1
           totalBytes:-1
      installProgress:-1
        errorCategory:ObjCErrorCategory(UpdateService::ErrorCategory::kNone)
                          .get()
            errorCode:0
            extraCode:0]);

  return ret;
}

StateChangeTestEngine::StatePair UpdateFoundStates(const std::string& app_id) {
  StateChangeTestEngine::StatePair ret;

  UpdateService::UpdateState& update_state = ret.second;
  update_state.app_id = app_id;
  update_state.state = UpdateService::UpdateState::State::kUpdateAvailable;
  update_state.next_version = base::Version("1.2.3.4");

  ret.first.reset([[CRUUpdateStateWrapper alloc]
        initWithAppId:base::SysUTF8ToNSString(app_id)
                state:ObjCState(
                          UpdateService::UpdateState::State::kUpdateAvailable)
                          .get()
              version:@"1.2.3.4"
      downloadedBytes:-1
           totalBytes:-1
      installProgress:-1
        errorCategory:ObjCErrorCategory(UpdateService::ErrorCategory::kNone)
                          .get()
            errorCode:0
            extraCode:0]);
  return ret;
}

StateChangeTestEngine::StatePair DownloadingStates(const std::string& app_id,
                                                   int64_t downloaded_bytes,
                                                   int64_t total_bytes) {
  StateChangeTestEngine::StatePair ret;

  UpdateService::UpdateState& update_state = ret.second;
  update_state.app_id = app_id;
  update_state.state = UpdateService::UpdateState::State::kDownloading;
  update_state.next_version = base::Version("1.2.3.4");
  update_state.downloaded_bytes = downloaded_bytes;
  update_state.total_bytes = total_bytes;

  ret.first.reset([[CRUUpdateStateWrapper alloc]
        initWithAppId:base::SysUTF8ToNSString(app_id)
                state:ObjCState(UpdateService::UpdateState::State::kDownloading)
                          .get()
              version:@"1.2.3.4"
      downloadedBytes:downloaded_bytes
           totalBytes:total_bytes
      installProgress:-1
        errorCategory:ObjCErrorCategory(UpdateService::ErrorCategory::kNone)
                          .get()
            errorCode:0
            extraCode:0]);
  return ret;
}

StateChangeTestEngine::StatePair InstallingStates(const std::string& app_id,
                                                  int64_t update_bytes,
                                                  int install_percentage) {
  StateChangeTestEngine::StatePair ret;

  UpdateService::UpdateState& update_state = ret.second;
  update_state.app_id = app_id;
  update_state.state = UpdateService::UpdateState::State::kInstalling;
  update_state.next_version = base::Version("1.2.3.4");
  update_state.downloaded_bytes = update_bytes;
  update_state.total_bytes = update_bytes;
  update_state.install_progress = install_percentage;

  ret.first.reset([[CRUUpdateStateWrapper alloc]
        initWithAppId:base::SysUTF8ToNSString(app_id)
                state:ObjCState(UpdateService::UpdateState::State::kInstalling)
                          .get()
              version:@"1.2.3.4"
      downloadedBytes:update_bytes
           totalBytes:update_bytes
      installProgress:install_percentage
        errorCategory:ObjCErrorCategory(UpdateService::ErrorCategory::kNone)
                          .get()
            errorCode:0
            extraCode:0]);
  return ret;
}

StateChangeTestEngine::StatePair UpdatedStates(const std::string& app_id,
                                               int64_t update_bytes) {
  StateChangeTestEngine::StatePair ret;

  UpdateService::UpdateState& update_state = ret.second;
  update_state.app_id = app_id;
  update_state.state = UpdateService::UpdateState::State::kUpdated;
  update_state.next_version = base::Version("1.2.3.4");
  update_state.downloaded_bytes = update_bytes;
  update_state.total_bytes = update_bytes;
  update_state.install_progress = 100;

  ret.first.reset([[CRUUpdateStateWrapper alloc]
        initWithAppId:base::SysUTF8ToNSString(app_id)
                state:ObjCState(UpdateService::UpdateState::State::kUpdated)
                          .get()
              version:@"1.2.3.4"
      downloadedBytes:update_bytes
           totalBytes:update_bytes
      installProgress:100
        errorCategory:ObjCErrorCategory(UpdateService::ErrorCategory::kNone)
                          .get()
            errorCode:0
            extraCode:0]);
  return ret;
}

#pragma mark Test cases
// TODO(crbug.com/1247504): Flaky on macOS 10.12.6.
TEST_F(MacUpdateServiceProxyTest, DISABLED_NoProductsUpdateAll) {
  ScopedXPCServiceMock::ConnectionMockRecord* conn_rec =
      mock_driver_.PrepareNewMockConnection();
  ScopedXPCServiceMock::RemoteObjectMockRecord* mock_rec =
      conn_rec->PrepareNewMockRemoteObject();
  id<CRUUpdateServicing> mock_remote_object = mock_rec->mock_object.get();

  OCMockBlockCapturer<void (^)(UpdateService::Result)> reply_block_capturer;
  // Create a pointer that can be copied into the .andDo block to refer to the
  // capturer so we can invoke the captured block.
  auto* reply_block_capturer_ptr = &reply_block_capturer;
  OCMExpect([mock_remote_object
                checkForUpdatesWithUpdateState:[OCMArg isNotNil]
                                         reply:reply_block_capturer.Capture()])
      .andDo(^(NSInvocation*) {
        reply_block_capturer_ptr->Get()[0].get()(
            UpdateService::Result::kAppNotFound);
      });

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this]() {
        service_->UpdateAll(
            base::BindRepeating(
                &ExpectNoCalls<const UpdateService::UpdateState&>,
                "no state updates expected"),
            base::BindLambdaForTesting(
                [this](UpdateService::Result actual_result) {
                  EXPECT_EQ(UpdateService::Result::kAppNotFound, actual_result);
                  run_loop_->QuitWhenIdle();
                }));
      }));

  run_loop_->Run();

  EXPECT_EQ(reply_block_capturer.Get().size(), 1UL);
  EXPECT_EQ(conn_rec->VendedObjectsCount(), 1UL);
  EXPECT_EQ(mock_driver_.VendedConnectionsCount(), 1UL);
  service_.reset();  // Drop service reference - should invalidate connection
  mock_driver_.VerifyAll();
}

TEST_F(MacUpdateServiceProxyTest, SimpleProductUpdate) {
  ScopedXPCServiceMock::ConnectionMockRecord* conn_rec =
      mock_driver_.PrepareNewMockConnection();
  ScopedXPCServiceMock::RemoteObjectMockRecord* mock_rec =
      conn_rec->PrepareNewMockRemoteObject();
  id<CRUUpdateServicing> mock_remote_object = mock_rec->mock_object.get();

  OCMockBlockCapturer<void (^)(UpdateService::Result)> reply_block_capturer;
  OCMockObjectCapturer<CRUUpdateStateObserver> update_state_observer_capturer;

  const std::string test_app_id("test_app_id");
  const std::string test_install_data_index("test_install_data_index");
  base::scoped_nsobject<CRUPriorityWrapper> wrapped_priority(
      [[CRUPriorityWrapper alloc]
          initWithPriority:UpdateService::Priority::kForeground]);
  base::scoped_nsobject<CRUPolicySameVersionUpdateWrapper>
      wrapped_policySameVersionUpdate([[CRUPolicySameVersionUpdateWrapper alloc]
          initWithPolicySameVersionUpdate:
              UpdateService::PolicySameVersionUpdate::kNotAllowed]);
  StateChangeTestEngine state_change_engine(
      std::vector<StateChangeTestEngine::StatePair>{
          CheckingForUpdatesStates(test_app_id), UpdateFoundStates(test_app_id),
          DownloadingStates(test_app_id, 1024, 655360),
          DownloadingStates(test_app_id, 2048, 655360),
          DownloadingStates(test_app_id, 327680, 655360),
          DownloadingStates(test_app_id, 655360, 655360),
          InstallingStates(test_app_id, 655360, 0),
          InstallingStates(test_app_id, 655360, 50),
          UpdatedStates(test_app_id, 655360)});

  // Blocks capture by copy. To capture referentially, explicitly create
  // unowned pointers to copy instead.
  auto* reply_block_capturer_ptr = &reply_block_capturer;
  auto* update_state_observer_capturer_ptr = &update_state_observer_capturer;
  auto* state_change_engine_ptr = &state_change_engine;
  OCMExpect([mock_remote_object
                checkForUpdateWithAppId:base::SysUTF8ToNSString(test_app_id)
                       installDataIndex:base::SysUTF8ToNSString(
                                            test_install_data_index)
                               priority:wrapped_priority.get()
                policySameVersionUpdate:wrapped_policySameVersionUpdate.get()
                            updateState:update_state_observer_capturer.Capture()
                                  reply:reply_block_capturer.Capture()])
      .andDo(^(NSInvocation*) {
        state_change_engine_ptr->StartSimulating(
            base::scoped_nsprotocol<id<CRUUpdateStateObserving>>(
                update_state_observer_capturer_ptr->Get()[0].get(),
                base::scoped_policy::RETAIN),
            base::BindOnce(reply_block_capturer_ptr->Get()[0],
                           UpdateService::Result::kSuccess));
      });

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this, &state_change_engine]() {
        service_->Update("test_app_id", "test_install_data_index",
                         UpdateService::Priority::kForeground,
                         UpdateService::PolicySameVersionUpdate::kNotAllowed,
                         state_change_engine.Watch(),
                         base::BindLambdaForTesting(
                             [this](UpdateService::Result actual_result) {
                               EXPECT_EQ(UpdateService::Result::kSuccess,
                                         actual_result);
                               run_loop_->QuitWhenIdle();
                             }));
      }));

  run_loop_->Run();

  EXPECT_EQ(reply_block_capturer.Get().size(), 1UL);
  EXPECT_EQ(conn_rec->VendedObjectsCount(), 1UL);
  EXPECT_EQ(mock_driver_.VendedConnectionsCount(), 1UL);
  service_.reset();  // Drop service reference - should invalidate connection
  mock_driver_.VerifyAll();
}

}  // namespace updater
