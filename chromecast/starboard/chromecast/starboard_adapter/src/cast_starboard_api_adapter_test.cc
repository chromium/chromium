// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cast_starboard_api.h>
#include <starboard/event.h>

#include <atomic>
#include <thread>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "chromecast/starboard/chromecast/starboard_adapter/src/cast_starboard_api_adapter_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockCastStarboardApi {
 public:
  MOCK_METHOD(int,
              SbRunStarboardMain,
              (int argc, char** argv, SbEventHandleCallback callback));
  MOCK_METHOD(void, SbSystemRequestStop, (int error_level));
  MOCK_METHOD(int,
              CastStarboardApiInitialize,
              (int argc, char** argv, SbEventHandleCB callback));
  MOCK_METHOD(void, CastStarboardApiFinalize, ());
  MOCK_METHOD(SbWindow, SbWindowCreate, (const SbWindowOptions* options));
};

MockCastStarboardApi* g_mock_cast_starboard_api = nullptr;

// Number of SbRunStarboardMain / CastStarboardApiInitialize trampoline
// invocations that have not returned.
//
// The adapter runs SbRunStarboardMain on a detached, unjoinable thread, so a
// test cannot join it. The counter is incremented before and decremented after
// the gmock call, i.e. strictly outside the frame in which gmock touches the
// mock object and its expectations. Waiting for it to drain therefore
// guarantees no detached thread is still inside gmock when the fixture's mock
// member is destroyed.
std::atomic_int g_sb_main_in_flight{0};

extern "C" {

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
    adapter->ResetForTesting();
    adapter->SetSbApiVersion(17);
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
  std::atomic_bool thread_ready_flag{false};

  SbEventHandleCallback sb_callback = nullptr;
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

  auto thread_body = [&]() {
    while (!thread_ready_flag.load(std::memory_order_relaxed)) {
    }
    auto* instance = CastStarboardApiAdapter::GetInstance();
    auto* id = reinterpret_cast<void*>(static_cast<uintptr_t>((instance_id++)));
    instance->Subscribe(id, nullptr);
    subscriber_count.fetch_add(1, std::memory_order_relaxed);
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
  CastStarboardApiAdapterImpl::GetInstance()->ReleaseForAtExit();
}

TEST_F(CastStarboardApiAdapterTest, ConcurrentGetsAndReleaseLegacySb14) {
  auto* adapter = CastStarboardApiAdapterImpl::GetInstance();
  adapter->SetSbApiVersion(14);

  SbEventHandleCB sb_callback = nullptr;
  EXPECT_CALL(cast_starboard_api_, CastStarboardApiInitialize(_, _, _))
      .Times(1)
      .WillOnce([&](int argc, char** argv, SbEventHandleCB callback) {
        sb_callback = callback;
        sb_callback(&fake_start_);
        return 0;
      });
  EXPECT_CALL(cast_starboard_api_, CastStarboardApiFinalize())
      .Times(1)
      .WillOnce([&]() { sb_callback(&fake_stop_); });

  void* context = reinterpret_cast<void*>(0x14);
  adapter->Subscribe(context, nullptr);
  adapter->Unsubscribe(context);
  adapter->ReleaseForAtExit();
}

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
  SbWindow window = adapter->GetWindow(nullptr);
  EXPECT_EQ(window, reinterpret_cast<SbWindow>(0x1234));

  // When the final subscriber unsubscribes, Starboard is cleanly stopped.
  adapter->Unsubscribe(context2);

  // Once Starboard is actually stopped, attempting to re-initialize it after
  // AtExit has run is forbidden and aborts.
  EXPECT_CHECK_DEATH(adapter->EnsureInitialized());
  EXPECT_CHECK_DEATH(adapter->Subscribe(context1, nullptr));
}

TEST_F(CastStarboardApiAdapterTest, NormalizesWindowSizeChangedEventOrdinal) {
  auto* adapter = CastStarboardApiAdapterImpl::GetInstance();
  adapter->SetSbApiVersion(18);

  SbEventHandleCallback sb_callback = nullptr;
  EXPECT_CALL(cast_starboard_api_, SbRunStarboardMain(_, _, _))
      .Times(1)
      .WillOnce([&](int, char**, SbEventHandleCallback callback) {
        sb_callback = callback;
        callback(&fake_start_);
        return 0;
      });
  EXPECT_CALL(cast_starboard_api_, SbSystemRequestStop(0))
      .Times(1)
      .WillOnce([&](int) { sb_callback(&fake_stop_); });

  struct ObservedEvents {
    std::vector<int> types;
    SbWindowSize last_size = {0, 0, 0.0f};
  } observed;

  adapter->Subscribe(
      &observed, +[](void* ctx, const SbEvent* event) {
        auto* out = static_cast<ObservedEvents*>(ctx);
        out->types.push_back(static_cast<int>(event->type));
        if (static_cast<int>(event->type) == 16 && event->data != nullptr) {
          out->last_size =
              static_cast<const SbEventWindowSizeChangedData*>(event->data)
                  ->size;
        }
      });

  // In Starboard 17/18, kSbEventTypeWindowSizeChanged has ordinal 13.
  SbEventWindowSizeChangedData size_data = {
      reinterpret_cast<SbWindow>(0x1234),
      {1080, 1920, 1.0f},
  };
  SbEvent window_size_event = {};
  window_size_event.type = static_cast<SbEventType>(13);
  window_size_event.data = &size_data;
  sb_callback(&window_size_event);

  ASSERT_EQ(observed.types.size(), 1u);
  // StarboardEventSource in core_runtime is compiled with base Starboard
  // headers (SB15/16) where kSbEventTypeWindowSizeChanged has ordinal 16.
  EXPECT_EQ(observed.types[0], 16);
  EXPECT_EQ(observed.last_size.width, 1080);
  EXPECT_EQ(observed.last_size.height, 1920);

  // Raw ordinal 16 in SB17/18 is kSbEventDateTimeConfigurationChanged and must
  // be normalized to its base SB15/16 ordinal (26), never forwarded as
  // kSbEventTypeWindowSizeChanged (16).
  SbEvent raw_ordinal_16_event = {};
  raw_ordinal_16_event.type = static_cast<SbEventType>(16);
  raw_ordinal_16_event.data = nullptr;
  sb_callback(&raw_ordinal_16_event);
  ASSERT_EQ(observed.types.size(), 2u);
  EXPECT_EQ(observed.types[1],
            static_cast<int>(kSbEventDateTimeConfigurationChanged));

  // Raw ordinal 17 in SB17/18 is
  // kSbEventTypeAccessibilityTextToSpeechSettingsChanged and must be normalized
  // to its base SB15/16 ordinal (23).
  SbEvent raw_ordinal_17_event = {};
  raw_ordinal_17_event.type = static_cast<SbEventType>(17);
  raw_ordinal_17_event.data = nullptr;
  sb_callback(&raw_ordinal_17_event);
  ASSERT_EQ(observed.types.size(), 3u);
  EXPECT_EQ(
      observed.types[2],
      static_cast<int>(kSbEventTypeAccessibilityTextToSpeechSettingsChanged));

  // When running on Starboard 16 (version < 17), event ordinals are not
  // shifted.
  adapter->SetSbApiVersion(16);
  SbEvent sb16_window_size_event = {};
  sb16_window_size_event.type = static_cast<SbEventType>(16);
  sb16_window_size_event.data = &size_data;
  sb_callback(&sb16_window_size_event);
  ASSERT_EQ(observed.types.size(), 4u);
  EXPECT_EQ(observed.types[3], 16);

  adapter->Unsubscribe(&observed);
  adapter->ReleaseForAtExit();
}

}  // namespace
}  // namespace chromecast
