// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_activity_url_filter_manager.h"

#include <atomic>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace chromecast {

class CastActivityUrlFilterManagerTest : public testing::Test {
 protected:
  void AddFilterForToken(const blink::LocalFrameToken& token)
      NO_THREAD_SAFETY_ANALYSIS {
    base::AutoLock lock(manager_.filters_lock_);
    manager_.activity_url_filters_.emplace(token, nullptr);
  }

  CastActivityUrlFilterManager manager_;
};

namespace {

class LookupRunner : public base::DelegateSimpleThread::Delegate {
 public:
  LookupRunner(CastActivityUrlFilterManager* manager,
               const blink::LocalFrameToken& token,
               base::WaitableEvent* started,
               std::atomic<bool>* stop)
      : manager_(manager), token_(token), started_(started), stop_(stop) {}

  void Run() override {
    started_->Signal();
    while (!stop_->load(std::memory_order_relaxed)) {
      EXPECT_EQ(nullptr,
                manager_->GetActivityUrlFilterForRenderFrameToken(token_));
    }
  }

 private:
  const raw_ptr<CastActivityUrlFilterManager> manager_;
  const blink::LocalFrameToken token_;
  const raw_ptr<base::WaitableEvent> started_;
  const raw_ptr<std::atomic<bool>> stop_;
};

}  // namespace

TEST_F(CastActivityUrlFilterManagerTest, LookupForUnknownTokenReturnsNull) {
  EXPECT_EQ(nullptr, manager_.GetActivityUrlFilterForRenderFrameToken(
                         blink::LocalFrameToken()));
}

// CastURLLoaderThrottleProvider clones may look up filters from worker
// threads while frames are being created and destroyed on the main thread,
// so the manager must allow concurrent lookups and map mutations.
TEST_F(CastActivityUrlFilterManagerTest, ConcurrentLookupAndModification) {
  constexpr size_t kEntryCount = 8;
  std::vector<blink::LocalFrameToken> tokens(kEntryCount);
  for (const auto& token : tokens) {
    AddFilterForToken(token);
  }

  blink::LocalFrameToken missing_token;
  base::WaitableEvent started;
  std::atomic<bool> stop(false);
  LookupRunner runner(&manager_, missing_token, &started, &stop);
  base::DelegateSimpleThread thread(&runner, "FilterLookup");
  thread.Start();
  started.Wait();

  constexpr size_t kIterations = 4096;
  for (size_t i = 0; i < kIterations; ++i) {
    AddFilterForToken(blink::LocalFrameToken());
  }
  for (const auto& token : tokens) {
    manager_.OnRenderFrameRemoved(token);
  }
  EXPECT_EQ(nullptr,
            manager_.GetActivityUrlFilterForRenderFrameToken(tokens[0]));

  stop.store(true, std::memory_order_relaxed);
  thread.Join();
}

}  // namespace chromecast
