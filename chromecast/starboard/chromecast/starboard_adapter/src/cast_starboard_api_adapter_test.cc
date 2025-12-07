// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/test/task_environment.h"
#include "chromecast/starboard/chromecast/starboard_adapter/src/cast_starboard_api_adapter_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= 15
#include <starboard/event.h>
#else  // SB_API_VERSION >=15
#include <cast_starboard_api.h>
#endif  // SB_API_VERSION >= 15

#include <atomic>
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

#if SB_API_VERSION >= 15
int SbRunStarboardMain(int argc, char** argv, SbEventHandleCallback callback) {
  return g_mock_cast_starboard_api->SbRunStarboardMain(argc, argv, callback);
}

void SbSystemRequestStop(int error_level) {
  g_mock_cast_starboard_api->SbSystemRequestStop(error_level);
}
#else   // SB_API_VERSION >= 15

int CastStarboardApiInitialize(int argc,
                               char** argv,
                               SbEventHandleCB callback) {
  return g_mock_cast_starboard_api->CastStarboardApiInitialize(argc, argv,
                                                               callback);
}

void CastStarboardApiFinalize() {
  g_mock_cast_starboard_api->CastStarboardApiFinalize();
}
#endif  // SB_API_VERSION >= 15

SbWindow SbWindowCreate(const SbWindowOptions* options) {
  return g_mock_cast_starboard_api->SbWindowCreate(options);
}

namespace chromecast {
namespace {

using ::testing::_;

class CastStarboardApiAdapterTest : public ::testing::Test {
 public:
  CastStarboardApiAdapterTest() {
    g_mock_cast_starboard_api = &cast_starboard_api_;
    fake_start_.type = kSbEventTypeStart;
    fake_stop_.type = kSbEventTypeStop;
  }

 protected:
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
  // Force any atexit calls to run now, while we have the mock objects around.
  // Without a manual call, this will occur outside of the test block.
  base::AtExitManager::ProcessCallbacksNow();
}

}  // namespace
}  // namespace chromecast
