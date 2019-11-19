// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
namespace web_cache {

class WebCacheManagerTest : public testing::Test {
 protected:
  typedef WebCacheManager::StatsMap StatsMap;
  typedef WebCacheManager::Allocation Allocation;
  typedef WebCacheManager::AllocationStrategy AllocationStrategy;

  static const int kRendererID;
  static const int kRendererID2;
  static const WebCacheManager::RendererInfo kStats;
  static const WebCacheManager::RendererInfo kStats2;

  WebCacheManagerTest() = default;

  // Thunks to access protected members of WebCacheManager
  static std::map<int, WebCacheManager::RendererInfo>& stats(
        WebCacheManager* h) {
    return h->stats_;
  }

  static void SimulateInactivity(WebCacheManager* h, int renderer_id) {
    stats(h)[renderer_id].access = Time::Now() - TimeDelta::FromMinutes(
        WebCacheManager::kRendererInactiveThresholdMinutes);
    h->FindInactiveRenderers();
  }

  static std::set<int>& active_renderers(WebCacheManager* h) {
    return h->active_renderers_;
  }
  static std::set<int>& inactive_renderers(WebCacheManager* h) {
    return h->inactive_renderers_;
  }
  static void GatherStats(WebCacheManager* h,
                          std::set<int> renderers,
                          WebCacheManager::RendererInfo* stats) {
    memset(stats, 0, sizeof(WebCacheManager::RendererInfo));
    h->GatherStats(renderers, &stats->capacity, &stats->size);
  }
  static uint64_t GetSize(int tactic,
                          const WebCacheManager::RendererInfo& stats) {
    return WebCacheManager::GetSize(
        static_cast<WebCacheManager::AllocationTactic>(tactic), stats.size);
  }
  static bool AttemptTactic(WebCacheManager* h,
                            int active_tactic,
                            const WebCacheManager::RendererInfo& active_stats,
                            int inactive_tactic,
                            const WebCacheManager::RendererInfo& inactive_stats,
                            std::list<std::pair<int, uint64_t>>* strategy) {
    return h->AttemptTactic(
        static_cast<WebCacheManager::AllocationTactic>(active_tactic),
        active_stats.size,
        static_cast<WebCacheManager::AllocationTactic>(inactive_tactic),
        inactive_stats.size, strategy);
  }
  static void AddToStrategy(WebCacheManager* h,
                            std::set<int> renderers,
                            int tactic,
                            uint64_t extra_bytes_to_allocate,
                            std::list<std::pair<int, uint64_t>>* strategy) {
    h->AddToStrategy(renderers,
                     static_cast<WebCacheManager::AllocationTactic>(tactic),
                     extra_bytes_to_allocate,
                     strategy);
  }

  static bool RendererInfoEqual(const WebCacheManager::RendererInfo& lhs,
                                const WebCacheManager::RendererInfo& rhs) {
    return lhs.capacity == rhs.capacity && lhs.size == rhs.size;
  }

  enum {
    DIVIDE_EVENLY = WebCacheManager::DIVIDE_EVENLY,
    KEEP_CURRENT_WITH_HEADROOM = WebCacheManager::KEEP_CURRENT_WITH_HEADROOM,
    KEEP_CURRENT = WebCacheManager::KEEP_CURRENT,
  };

  WebCacheManager* manager() { return &manager_; }

 private:
  // Create the environment before creating the WebCacheManager, because the
  // latter depends on the UI thread to have been set up correctly.
  content::BrowserTaskEnvironment task_environment_;
  WebCacheManager manager_;
};

// static
const int WebCacheManagerTest::kRendererID = 146;

// static
const int WebCacheManagerTest::kRendererID2 = 245;

// static
const WebCacheManager::RendererInfo WebCacheManagerTest::kStats = {
    base::Time(), 1024 * 1024, 256 * 1024 + 512,
};

// static
const WebCacheManager::RendererInfo WebCacheManagerTest::kStats2 = {
    base::Time(), 2 * 1024 * 1024, 2 * 256 * 1024 + 2 * 512,
};

TEST_F(WebCacheManagerTest, AddRemoveRendererTest) {
  EXPECT_EQ(0U, active_renderers(manager()).size());
  EXPECT_EQ(0U, inactive_renderers(manager()).size());

  manager()->Add(kRendererID);
  EXPECT_EQ(1U, active_renderers(manager()).count(kRendererID));
  EXPECT_EQ(0U, inactive_renderers(manager()).count(kRendererID));

  manager()->Remove(kRendererID);
  EXPECT_EQ(0U, active_renderers(manager()).size());
  EXPECT_EQ(0U, inactive_renderers(manager()).size());
}

TEST_F(WebCacheManagerTest, ActiveInactiveTest) {
  manager()->Add(kRendererID);

  manager()->ObserveActivity(kRendererID);
  EXPECT_EQ(1U, active_renderers(manager()).count(kRendererID));
  EXPECT_EQ(0U, inactive_renderers(manager()).count(kRendererID));

  SimulateInactivity(manager(), kRendererID);
  EXPECT_EQ(0U, active_renderers(manager()).count(kRendererID));
  EXPECT_EQ(1U, inactive_renderers(manager()).count(kRendererID));

  manager()->ObserveActivity(kRendererID);
  EXPECT_EQ(1U, active_renderers(manager()).count(kRendererID));
  EXPECT_EQ(0U, inactive_renderers(manager()).count(kRendererID));

  manager()->Remove(kRendererID);
}

TEST_F(WebCacheManagerTest, ObserveStatsTest) {
  manager()->Add(kRendererID);

  EXPECT_EQ(1U, stats(manager()).size());

  manager()->ObserveStats(kRendererID, kStats.capacity, kStats.size);

  EXPECT_EQ(1U, stats(manager()).size());
  EXPECT_TRUE(RendererInfoEqual(kStats, stats(manager())[kRendererID]));

  manager()->Remove(kRendererID);
}

TEST_F(WebCacheManagerTest, SetGlobalSizeLimitTest) {
  uint64_t limit = manager()->GetDefaultGlobalSizeLimit();
  manager()->SetGlobalSizeLimit(limit);
  EXPECT_EQ(limit, manager()->global_size_limit());

  manager()->SetGlobalSizeLimit(0);
  EXPECT_EQ(0U, manager()->global_size_limit());
}

TEST_F(WebCacheManagerTest, GatherStatsTest) {
  manager()->Add(kRendererID);
  manager()->Add(kRendererID2);

  manager()->ObserveStats(kRendererID, kStats.capacity, kStats.size);
  manager()->ObserveStats(kRendererID2, kStats2.capacity, kStats2.size);

  std::set<int> renderer_set;
  renderer_set.insert(kRendererID);

  WebCacheManager::RendererInfo stats;
  GatherStats(manager(), renderer_set, &stats);

  EXPECT_TRUE(RendererInfoEqual(kStats, stats));

  renderer_set.insert(kRendererID2);
  GatherStats(manager(), renderer_set, &stats);

  WebCacheManager::RendererInfo expected_stats = kStats;
  expected_stats.capacity += kStats2.capacity;
  expected_stats.size += kStats2.size;

  EXPECT_TRUE(RendererInfoEqual(expected_stats, stats));

  manager()->Remove(kRendererID);
  manager()->Remove(kRendererID2);
}

TEST_F(WebCacheManagerTest, GetSizeTest) {
  EXPECT_EQ(0U, GetSize(DIVIDE_EVENLY, kStats));
  EXPECT_LT(256 * 1024u + 512, GetSize(KEEP_CURRENT_WITH_HEADROOM, kStats));
  EXPECT_EQ(256 * 1024u + 512, GetSize(KEEP_CURRENT, kStats));
}

TEST_F(WebCacheManagerTest, AttemptTacticTest) {
  manager()->Add(kRendererID);
  manager()->Add(kRendererID2);

  manager()->ObserveActivity(kRendererID);
  SimulateInactivity(manager(), kRendererID2);

  manager()->ObserveStats(kRendererID, kStats.capacity, kStats.size);
  manager()->ObserveStats(kRendererID2, kStats2.capacity, kStats2.size);

  AllocationStrategy strategy;

  manager()->SetGlobalSizeLimit(kStats.size + kStats2.size - 1);
  EXPECT_FALSE(AttemptTactic(manager(),
                             KEEP_CURRENT,
                             kStats,
                             KEEP_CURRENT,
                             kStats2,
                             &strategy));
  EXPECT_TRUE(strategy.empty());

  manager()->SetGlobalSizeLimit(kStats.size + kStats2.size);
  EXPECT_TRUE(AttemptTactic(manager(), KEEP_CURRENT, kStats, KEEP_CURRENT,
                            kStats2, &strategy));
  EXPECT_EQ(2U, strategy.size());

  auto iter = strategy.begin();
  while (iter != strategy.end()) {
    if (iter->first == kRendererID)
      EXPECT_LE(kStats.size, iter->second);
    else if (iter->first == kRendererID2)
      EXPECT_LE(kStats2.size, iter->second);
    else
      ADD_FAILURE();   // Unexpected entry in strategy.
    ++iter;
  }

  manager()->Remove(kRendererID);
  manager()->Remove(kRendererID2);
}

TEST_F(WebCacheManagerTest, AddToStrategyTest) {
  manager()->Add(kRendererID);
  manager()->Add(kRendererID2);

  std::set<int> renderer_set;
  renderer_set.insert(kRendererID);
  renderer_set.insert(kRendererID2);

  manager()->ObserveStats(kRendererID, kStats.capacity, kStats.size);
  manager()->ObserveStats(kRendererID2, kStats2.capacity, kStats2.size);

  const uint64_t kExtraBytesToAllocate = 10 * 1024;

  AllocationStrategy strategy;
  AddToStrategy(manager(),
                renderer_set,
                KEEP_CURRENT,
                kExtraBytesToAllocate,
                &strategy);

  EXPECT_EQ(2U, strategy.size());

  uint64_t total_bytes = 0;
  auto iter = strategy.begin();
  while (iter != strategy.end()) {
    total_bytes += iter->second;

    if (iter->first == kRendererID)
      EXPECT_LE(kStats.size, iter->second);
    else if (iter->first == kRendererID2)
      EXPECT_LE(kStats2.size, iter->second);
    else
      ADD_FAILURE();  // Unexpected entry in strategy.
    ++iter;
  }

  uint64_t expected_total_bytes =
      kExtraBytesToAllocate + kStats.size + kStats2.size;

  EXPECT_GE(expected_total_bytes, total_bytes);

  manager()->Remove(kRendererID);
  manager()->Remove(kRendererID2);
}

// Regression test for http://crbug.com/12362.
// There are three operations in the following order will cause the crash:
// Remove(kRendererID) -> ObserveActivity(kRendererID) -> Remove(kRendererID2)
// To prevent similar failures in the future, 6 tests are added in total to
// cover all the possible orderings of these three operations.
TEST_F(WebCacheManagerTest,
       CallRemoveRendererAndObserveActivityInAnyOrderShouldNotCrashTest_1) {
  EXPECT_EQ(0U, active_renderers(manager()).size());
  EXPECT_EQ(0U, inactive_renderers(manager()).size());

  // Add, Remove, and ObserveActivity trigger deferred
  // calls to ReviseAllocationStrategy and that we call it directly after each
  // operation to sidestep the need to wait for an unobservable background
  // operation.
  manager()->Add(kRendererID);
  manager()->ReviseAllocationStrategy();
  manager()->Add(kRendererID2);
  manager()->ReviseAllocationStrategy();

  // The following order will cause a crash in http://crbug.com/12362.
  manager()->Remove(kRendererID);
  manager()->ReviseAllocationStrategy();

  manager()->ObserveActivity(kRendererID);
  manager()->ReviseAllocationStrategy();

  manager()->Remove(kRendererID2);
  manager()->ReviseAllocationStrategy();
}

TEST_F(WebCacheManagerTest,
       CallRemoveRendererAndObserveActivityInAnyOrderShouldNotCrashTest_2) {
  EXPECT_EQ(0U, active_renderers(manager()).size());
  EXPECT_EQ(0U, inactive_renderers(manager()).size());

  // Add, Remove, and ObserveActivity trigger deferred
  // calls to ReviseAllocationStrategy and that we call it directly after each
  // operation to sidestep the need to wait for an unobservable background
  // operation.
  manager()->Add(kRendererID);
  manager()->ReviseAllocationStrategy();
  manager()->Add(kRendererID2);
  manager()->ReviseAllocationStrategy();

  manager()->Remove(kRendererID);
  manager()->ReviseAllocationStrategy();

  manager()->Remove(kRendererID2);
  manager()->ReviseAllocationStrategy();

  manager()->ObserveActivity(kRendererID);
  manager()->ReviseAllocationStrategy();

}

TEST_F(WebCacheManagerTest,
       CallRemoveRendererAndObserveActivityInAnyOrderShouldNotCrashTest_3) {
  EXPECT_EQ(0U, active_renderers(manager()).size());
  EXPECT_EQ(0U, inactive_renderers(manager()).size());

  // Add, Remove, and ObserveActivity trigger deferred
  // calls to ReviseAllocationStrategy and that we call it directly after each
  // operation to sidestep the need to wait for an unobservable background
  // operation.
  manager()->Add(kRendererID);
  manager()->ReviseAllocationStrategy();
  manager()->Add(kRendererID2);
  manager()->ReviseAllocationStrategy();

  manager()->ObserveActivity(kRendererID);
  EXPECT_EQ(0U, inactive_renderers(manager()).size());

  manager()->Remove(kRendererID);
  manager()->ReviseAllocationStrategy();

  manager()->Remove(kRendererID2);
  manager()->ReviseAllocationStrategy();
}

TEST_F(WebCacheManagerTest,
       CallRemoveRendererAndObserveActivityInAnyOrderShouldNotCrashTest_4) {
  EXPECT_EQ(0U, active_renderers(manager()).size());
  EXPECT_EQ(0U, inactive_renderers(manager()).size());

  // Add, Remove, and ObserveActivity trigger deferred
  // calls to ReviseAllocationStrategy and that we call it directly after each
  // operation to sidestep the need to wait for an unobservable background
  // operation.
  manager()->Add(kRendererID);
  manager()->ReviseAllocationStrategy();
  manager()->Add(kRendererID2);
  manager()->ReviseAllocationStrategy();

  manager()->ObserveActivity(kRendererID);
  EXPECT_EQ(0U, inactive_renderers(manager()).size());

  manager()->Remove(kRendererID2);
  manager()->ReviseAllocationStrategy();

  manager()->Remove(kRendererID);
  manager()->ReviseAllocationStrategy();
}

TEST_F(WebCacheManagerTest,
       CallRemoveRendererAndObserveActivityInAnyOrderShouldNotCrashTest_5) {
  EXPECT_EQ(0U, active_renderers(manager()).size());
  EXPECT_EQ(0U, inactive_renderers(manager()).size());

  // Add, Remove, and ObserveActivity trigger deferred
  // calls to ReviseAllocationStrategy and that we call it directly after each
  // operation to sidestep the need to wait for an unobservable background
  // operation.
  manager()->Add(kRendererID);
  manager()->ReviseAllocationStrategy();
  manager()->Add(kRendererID2);
  manager()->ReviseAllocationStrategy();

  manager()->Remove(kRendererID2);
  manager()->ReviseAllocationStrategy();

  manager()->ObserveActivity(kRendererID);
  manager()->ReviseAllocationStrategy();

  manager()->Remove(kRendererID);
  manager()->ReviseAllocationStrategy();
}

TEST_F(WebCacheManagerTest,
       CallRemoveRendererAndObserveActivityInAnyOrderShouldNotCrashTest_6) {
  EXPECT_EQ(0U, active_renderers(manager()).size());
  EXPECT_EQ(0U, inactive_renderers(manager()).size());

  // Add, Remove, and ObserveActivity trigger deferred
  // calls to ReviseAllocationStrategy and that we call it directly after each
  // operation to sidestep the need to wait for an unobservable background
  // operation.
  manager()->Add(kRendererID);
  manager()->ReviseAllocationStrategy();
  manager()->Add(kRendererID2);
  manager()->ReviseAllocationStrategy();

  manager()->Remove(kRendererID2);
  manager()->ReviseAllocationStrategy();

  manager()->Remove(kRendererID);
  manager()->ReviseAllocationStrategy();

  manager()->ObserveActivity(kRendererID);
  manager()->ReviseAllocationStrategy();
}

}  // namespace web_cache
