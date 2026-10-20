// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "chromecast/starboard/chromecast/starboard_adapter/src/cast_starboard_api_adapter_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= 15
#include <starboard/event.h>
#else  // SB_API_VERSION >=15
#include <cast_starboard_api.h>
#endif  // SB_API_VERSION >= 15

#include <atomic>
// <thread> is normally banned by styleguide/c++/c++-features.md, but this test
// deliberately mirrors the code under test: the adapter is linked into
// libcast_starboard_adapter.so.V, which carries its own //base, so it starts
// Starboard with a raw std::thread rather than base::Thread. Do not "fix"
// these to base::Thread.
#include <thread>

class MockCastStarboardApi {
 public:
#if SB_API_VERSION >= 15
  MOCK_METHOD(int,
              SbRunStarboardMain,
              (int argc, char** argv, SbEventHandleCallback callback));
  MOCK_METHOD(void, SbSystemRequestStop, (int error_level));
#else
  MOCK_METHOD(int,
              CastStarboardApiInitialize,
              (int argc, char** argv, SbEventHandleCB callback));
  MOCK_METHOD(void, CastStarboardApiFinalize, ());
#endif
  MOCK_METHOD(SbWindow, SbWindowCreate, (const SbWindowOptions* options));
};

MockCastStarboardApi* g_mock_cast_starboard_api = nullptr;

// Number of SbRunStarboardMain trampoline invocations that have not returned.
//
// The adapter runs SbRunStarboardMain on a detached, unjoinable thread, so a
// test cannot join it. The counter is incremented before and decremented after
// the gmock call, i.e. strictly outside the frame in which gmock touches the
// mock object and its expectations. Waiting for it to drain therefore
// guarantees no detached thread is still inside gmock when the fixture's mock
// member is destroyed.
std::atomic_int g_sb_main_in_flight{0};

extern "C" {

#if SB_API_VERSION >= 15
__attribute__((visibility("default"))) int
SbRunStarboardMain(int argc, char** argv, SbEventHandleCallback callback) {
  g_sb_main_in_flight.fetch_add(1, std::memory_order_acquire);
  const int result =
      g_mock_cast_starboard_api->SbRunStarboardMain(argc, argv, callback);
  g_sb_main_in_flight.fetch_sub(1, std::memory_order_release);
  return result;
}

__attribute__((visibility("default"))) void SbSystemRequestStop(
    int error_level) {
  g_mock_cast_starboard_api->SbSystemRequestStop(error_level);
}
#else   // SB_API_VERSION >= 15

__attribute__((visibility("default"))) int
CastStarboardApiInitialize(int argc, char** argv, SbEventHandleCB callback) {
  g_sb_main_in_flight.fetch_add(1, std::memory_order_acquire);
  const int result = g_mock_cast_starboard_api->CastStarboardApiInitialize(
      argc, argv, callback);
  g_sb_main_in_flight.fetch_sub(1, std::memory_order_release);
  return result;
}

__attribute__((visibility("default"))) void CastStarboardApiFinalize() {
  g_mock_cast_starboard_api->CastStarboardApiFinalize();
}
#endif  // SB_API_VERSION >= 15

__attribute__((visibility("default"))) SbWindow
SbWindowCreate(const SbWindowOptions* options) {
  return g_mock_cast_starboard_api->SbWindowCreate(options);
}

}  // extern "C"

namespace chromecast {
namespace {

using ::testing::_;

class CastStarboardApiAdapterTest : public ::testing::Test {
 public:
  CastStarboardApiAdapterTest() {
    g_mock_cast_starboard_api = &cast_starboard_api_;
    fake_start_.type = kSbEventTypeStart;
    fake_stop_.type = kSbEventTypeStop;

    // The adapter singleton is intentionally immortal, so its state outlives an
    // individual test. Normalize it so tests batched into one process by the
    // unit test launcher do not contaminate each other.
    ResetAdapterState();
  }

  ~CastStarboardApiAdapterTest() override {
    // The detached Starboard-main threads call into `cast_starboard_api_`,
    // which is a member and is destroyed immediately after this body runs.
    // Wait for them to leave the trampoline (and therefore gmock) first;
    // they are unjoinable, so this is the only way to order the two.
    while (g_sb_main_in_flight.load(std::memory_order_acquire) != 0) {
      base::PlatformThread::YieldCurrentThread();
    }
    g_mock_cast_starboard_api = nullptr;
  }

 protected:
  static CastStarboardApiAdapterImpl* ResetAdapterState() {
    auto* adapter = CastStarboardApiAdapterImpl::GetInstance();
    base::AutoLock autolock(adapter->lock_);
    adapter->subscribers_.clear();
    adapter->window_ = kSbWindowInvalid;
    adapter->initialized_ = false;
    adapter->released_ = false;
    adapter->at_exit_ran_ = false;
    adapter->starboard_started_.Reset();
    adapter->starboard_stopped_.Reset();
    return adapter;
  }

  MockCastStarboardApi cast_starboard_api_;
  SbEvent fake_start_;
  SbEvent fake_stop_;
};

TEST_F(CastStarboardApiAdapterTest, ConcurrentGetsAndRelease) {
  constexpr int num_threads = 10;
  std::atomic_int32_t instance_id{0};
  std::atomic_int32_t subscriber_count{0};
  std::atomic_bool thread_ready_flag;
  // Init/Main and Finalize/Stop should onnly be called once, but
  // due to the way the adapter is implemented, if the code under test
  // is executed multiple times, and we do not send the expected
  // Start/Stop signals, the tests will hang waiting on a future to
  // complete.
#if SB_API_VERSION >= 15
  SbEventHandleCallback sb_callback;
  EXPECT_CALL(cast_starboard_api_, SbRunStarboardMain(_, _, _))
      .Times(1)
      .WillRepeatedly(
          [&](int argc, char** argv, SbEventHandleCallback callback) {
            sb_callback = callback;
            sb_callback(&fake_start_);
            return 0;
          });
  EXPECT_CALL(cast_starboard_api_, SbSystemRequestStop(0))
      .Times(1)
      .WillRepeatedly([&]() { sb_callback(&fake_stop_); });
#else
  SbEventHandleCB sb_callback;
  EXPECT_CALL(cast_starboard_api_, CastStarboardApiInitialize(_, _, _))
      .Times(1)
      .WillRepeatedly([&](int argc, char** argv, SbEventHandleCB callback) {
        sb_callback = callback;
        sb_callback(&fake_start_);
        return 0;
      });
  EXPECT_CALL(cast_starboard_api_, CastStarboardApiFinalize())
      .Times(1)
      .WillRepeatedly([&]() { sb_callback(&fake_stop_); });
#endif

  auto thread_body = [&]() {
    // Busy loop until all threads are init so we can have as much overlap as
    // possible on the following code paths.
    while (!thread_ready_flag.load(std::memory_order_relaxed)) {
    }
    auto* instance = CastStarboardApiAdapter::GetInstance();
    auto* id = reinterpret_cast<void*>(static_cast<uintptr_t>((instance_id++)));
    instance->Subscribe(id, nullptr);
    subscriber_count.fetch_add(1, std::memory_order_relaxed);
    // Spin until all subscribers have init. we're only testing for races with
    // Initialize/Release, not interleavings currently.
    while (subscriber_count.load(std::memory_order_relaxed) != num_threads) {
    }
    instance->Unsubscribe(id);
  };
  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.push_back(std::thread(thread_body));
  }
  thread_ready_flag.store(true, std::memory_order_relaxed);
  for (auto& thread : threads) {
    thread.join();
  }
  // Run the teardown now, while we have the mock objects around. In production
  // this is driven by the base::AtExitManager task that StarboardAdapterLoader
  // registers in the executable; this test links the adapter source_set
  // directly, so it invokes the same entry point by hand.
  CastStarboardApiAdapterImpl::GetInstance()->ReleaseForAtExit();
}

#if SB_API_VERSION >= 15
// Release() must never discard the kSbEventTypeStart signal of a Starboard
// instance that is still running: Starboard only emits that event once, so
// every later EnsureInitialized() caller would block forever.
TEST_F(CastStarboardApiAdapterTest, ReleaseDoesNotDiscardLiveStartSignal) {
  auto* adapter = CastStarboardApiAdapterImpl::GetInstance();
  base::WaitableEvent stop_entered;
  base::WaitableEvent allow_stop;
  base::WaitableEvent restarted;

  // Two starts: the initial one, plus the restart that races the in-flight
  // Release() below.
  EXPECT_CALL(cast_starboard_api_, SbRunStarboardMain(_, _, _))
      .Times(2)
      .WillOnce([&](int, char**, SbEventHandleCallback callback) {
        callback(&fake_start_);
        return 0;
      })
      .WillOnce([&](int, char**, SbEventHandleCallback callback) {
        callback(&fake_start_);
        restarted.Signal();
        return 0;
      });
  // Park inside the stop request so the test controls exactly when Release()
  // runs its teardown tail.
  EXPECT_CALL(cast_starboard_api_, SbSystemRequestStop(0))
      .Times(1)
      .WillOnce([&](int) {
        stop_entered.Signal();
        allow_stop.Wait();
        CastStarboardApiAdapterImpl::SbEventHandle(&fake_stop_);
      });

  adapter->EnsureInitialized();

  std::thread releaser([adapter]() { adapter->Release(); });
  stop_entered.Wait();

  // Starboard is restarted while the Release() above is still in flight, so it
  // is running (and initialized_ is true) once Release() finishes.
  adapter->EnsureInitialized();
  restarted.Wait();

  allow_stop.Signal();
  releaser.join();

  base::WaitableEvent reinitialized;
  std::thread waiter([adapter, &reinitialized]() {
    adapter->EnsureInitialized();
    reinitialized.Signal();
  });
  const bool completed =
      reinitialized.TimedWait(TestTimeouts::action_timeout());
  if (!completed) {
    // Unwedge the thread so the test process can shut down.
    adapter->starboard_started_.Signal();
  }
  waiter.join();
  EXPECT_TRUE(completed)
      << "EnsureInitialized() hung: Release() discarded the start signal of a "
         "Starboard instance that is still running.";
}

// Once the AtExitManager teardown has run, Starboard must never be restarted:
// a second SbRunStarboardMain is out of contract for the partner library.
TEST_F(CastStarboardApiAdapterTest, StarboardIsNotRestartedAfterAtExit) {
  auto* adapter = CastStarboardApiAdapterImpl::GetInstance();
  EXPECT_CALL(cast_starboard_api_, SbRunStarboardMain(_, _, _))
      .Times(1)
      .WillOnce([&](int, char**, SbEventHandleCallback callback) {
        callback(&fake_start_);
        return 0;
      });
  EXPECT_CALL(cast_starboard_api_, SbSystemRequestStop(0))
      .Times(1)
      .WillOnce([&](int) {
        CastStarboardApiAdapterImpl::SbEventHandle(&fake_stop_);
      });

  adapter->EnsureInitialized();
  adapter->ReleaseForAtExit();

  EXPECT_CHECK_DEATH(adapter->EnsureInitialized());
}

// When active subscribers remain during AtExitManager teardown, Starboard
// remains running and late callers can still call GetWindow() without crashing.
TEST_F(CastStarboardApiAdapterTest,
       GetWindowSucceedsDuringTeardownWithSubscribers) {
  auto* adapter = CastStarboardApiAdapterImpl::GetInstance();
  SbEventHandleCallback sb_callback;
  EXPECT_CALL(cast_starboard_api_, SbRunStarboardMain(_, _, _))
      .Times(1)
      .WillOnce([&](int, char**, SbEventHandleCallback callback) {
        sb_callback = callback;
        sb_callback(&fake_start_);
        return 0;
      });
  EXPECT_CALL(cast_starboard_api_, SbWindowCreate(_))
      .Times(1)
      .WillOnce(testing::Return(reinterpret_cast<SbWindow>(0x1234)));
  EXPECT_CALL(cast_starboard_api_, SbSystemRequestStop(0))
      .Times(1)
      .WillOnce([&](int) { sb_callback(&fake_stop_); });

  void* context1 = reinterpret_cast<void*>(0x1);
  adapter->Subscribe(context1, nullptr);

  // AtExitManager runs while there is still an active subscriber.
  // Release() defers stopping Starboard because subscribers_ is not empty.
  adapter->ReleaseForAtExit();

  // A nested subscriber (e.g. StarboardDrmWrapper initialized by media
  // pipeline) subscribes while Starboard is still running during teardown,
  // and the original subscriber unsubscribes before GetWindow() is called.
  void* context2 = reinterpret_cast<void*>(0x2);
  adapter->Subscribe(context2, nullptr);
  adapter->Unsubscribe(context1);

  // A late thread (e.g. media pipeline / CreatePlayer) requests the window.
  // Before the fix, EnsureInitialized() unconditionally CHECKed
  // (!at_exit_ran_), causing a SIGTRAP crash here.
  SbWindow window = adapter->GetWindow(nullptr);
  EXPECT_EQ(window, reinterpret_cast<SbWindow>(0x1234));

  // When the final subscriber unsubscribes, Starboard is cleanly stopped.
  adapter->Unsubscribe(context2);

  // Once Starboard is actually stopped, attempting to re-initialize it after
  // AtExit has run is forbidden and aborts.
  EXPECT_CHECK_DEATH(adapter->EnsureInitialized());
  EXPECT_CHECK_DEATH(adapter->Subscribe(context1, nullptr));
}
#endif  // SB_API_VERSION >= 15

}  // namespace
}  // namespace chromecast
