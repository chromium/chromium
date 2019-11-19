// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the browser side of the cache manager, it tracks the activity of the
// render processes and allocates available memory cache resources.

#ifndef COMPONENTS_WEB_CACHE_BROWSER_WEB_CACHE_MANAGER_H_
#define COMPONENTS_WEB_CACHE_BROWSER_WEB_CACHE_MANAGER_H_

#include <stddef.h>

#include <list>
#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "components/web_cache/public/mojom/web_cache.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_creation_observer.h"
#include "content/public/browser/render_process_host_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace web_cache {

// Note: memory usage uses uint64_t because potentially the browser could be
// 32 bit and the renderers 64 bits.
class WebCacheManager : public content::RenderProcessHostCreationObserver,
                        public content::RenderProcessHostObserver {
  friend class WebCacheManagerTest;
  FRIEND_TEST_ALL_PREFIXES(
      WebCacheManagerTest,
      GatherStatsTest);
  FRIEND_TEST_ALL_PREFIXES(
      WebCacheManagerTest,
      CallRemoveRendererAndObserveActivityInAnyOrderShouldNotCrashTest_1);
  FRIEND_TEST_ALL_PREFIXES(
      WebCacheManagerTest,
      CallRemoveRendererAndObserveActivityInAnyOrderShouldNotCrashTest_2);
  FRIEND_TEST_ALL_PREFIXES(
      WebCacheManagerTest,
      CallRemoveRendererAndObserveActivityInAnyOrderShouldNotCrashTest_3);
  FRIEND_TEST_ALL_PREFIXES(
      WebCacheManagerTest,
      CallRemoveRendererAndObserveActivityInAnyOrderShouldNotCrashTest_4);
  FRIEND_TEST_ALL_PREFIXES(
      WebCacheManagerTest,
      CallRemoveRendererAndObserveActivityInAnyOrderShouldNotCrashTest_5);
  FRIEND_TEST_ALL_PREFIXES(
      WebCacheManagerTest,
      CallRemoveRendererAndObserveActivityInAnyOrderShouldNotCrashTest_6);

 public:
  // Gets the singleton WebCacheManager object.  The first time this method
  // is called, a WebCacheManager object is constructed and returned.
  // Subsequent calls will return the same object.
  static WebCacheManager* GetInstance();

  // When a render process is created, it registers itself with the cache
  // manager host, causing the renderer to be allocated cache resources.
  void Add(int renderer_id);

  // When a render process ends, it removes itself from the cache manager host,
  // freeing the manager to assign its cache resources to other renderers.
  void Remove(int renderer_id);

  // The cache manager assigns more cache resources to active renderer.  When a
  // renderer is active, it should inform the cache manager to receive more
  // cache resources.
  //
  // When a renderer moves from being inactive to being active, the cache
  // manager may decide to adjust its resource allocation, but it will delay
  // the recalculation, allowing ObserveActivity to return quickly.
  void ObserveActivity(int renderer_id);

  // Periodically, renderers should inform the cache manager of their current
  // statistics.  The more up-to-date the cache manager's statistics, the
  // better it can allocate cache resources.
  void ObserveStats(int renderer_id, uint64_t capacity, uint64_t size);

  // The global limit on the number of bytes in all the in-memory caches.
  uint64_t global_size_limit() const { return global_size_limit_; }

  // Sets the global size limit, forcing a recalculation of cache allocations.
  void SetGlobalSizeLimit(uint64_t bytes);

  // Clears all in-memory caches.
  void ClearCache();

  // Instantly clears renderer cache for a process.
  // Must be called between Add(process_id) and Remove(process_id).
  void ClearCacheForProcess(int process_id);

  // Clears all in-memory caches when a tab is reloaded or the user navigates
  // to a different website.
  void ClearCacheOnNavigation();

  // content::RenderProcessHostCreationObserver:
  void OnRenderProcessHostCreated(
      content::RenderProcessHost* process_host) override;

  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Gets the default global size limit.  This interrogates system metrics to
  // tune the default size to the current system.
  static uint64_t GetDefaultGlobalSizeLimit();

 protected:
  // The amount of idle time before we consider a tab to be "inactive"
  static const int kRendererInactiveThresholdMinutes = 5;

  // Keep track of some renderer information.
  struct RendererInfo {
    // The access time for this renderer.
    base::Time access;
    uint64_t capacity;
    uint64_t size;
  };

  typedef std::map<int, RendererInfo> StatsMap;

  // An allocation is the number of bytes a specific renderer should use for
  // its cache.
  typedef std::pair<int,uint64_t> Allocation;

  // An allocation strategy is a list of allocations specifying the resources
  // each renderer is permitted to consume for its cache.
  typedef std::list<Allocation> AllocationStrategy;

  // The key is the unique id of every render process host.
  typedef std::map<int, mojo::Remote<mojom::WebCache>> WebCacheServicesMap;

  // This class is a singleton.  Do not instantiate directly. Call GetInstance()
  // instead.
  WebCacheManager();
  friend class base::NoDestructor<WebCacheManager>;

  ~WebCacheManager() override;

  // Recomputes the allocation of cache resources among the renderers.  Also
  // informs the renderers of their new allocation.
  void ReviseAllocationStrategy();

  // Schedules a call to ReviseAllocationStrategy after a short delay.
  void ReviseAllocationStrategyLater();

  // The various tactics used as part of an allocation strategy.  To decide
  // how many resources a given renderer should be allocated, we consider its
  // usage statistics.  Each tactic specifies the function that maps usage
  // statistics to resource allocations.
  //
  // Determining a resource allocation strategy amounts to picking a tactic
  // for each renderer and checking that the total memory required fits within
  // our |global_size_limit_|.
  enum AllocationTactic {
    // Ignore cache statistics and divide resources equally among the given
    // set of caches.
    DIVIDE_EVENLY,

    // Allow each renderer to keep its current set of cached resources, with
    // some extra allocation to store new objects.
    KEEP_CURRENT_WITH_HEADROOM,

    // Allow each renderer to keep its current set of cached resources.
    KEEP_CURRENT,
  };

  // Helper functions for devising an allocation strategy

  // Add up all the stats from the given set of renderers and place the result
  // in the given parameters.
  void GatherStats(const std::set<int>& renderers,
                   uint64_t* capacity,
                   uint64_t* size);

  // Get the amount of memory that would be required to implement |tactic|
  // using the specified allocation tactic.  This function defines the
  // semantics for each of the tactics.
  static uint64_t GetSize(AllocationTactic tactic, uint64_t size);

  // Attempt to use the specified tactics to compute an allocation strategy
  // and place the result in |strategy|.  |active_stats| and |inactive_stats|
  // are the aggregate statistics for |active_renderers_| and
  // |inactive_renderers_|, respectively.
  //
  // Returns |true| on success and |false| on failure.  Does not modify
  // |strategy| on failure.
  bool AttemptTactic(AllocationTactic active_tactic,
                     uint64_t active_size,
                     AllocationTactic inactive_tactic,
                     uint64_t inactive_size,
                     AllocationStrategy* strategy);

  // For each renderer in |renderers|, computes its allocation according to
  // |tactic| and add the result to |strategy|.  Any |extra_bytes_to_allocate|
  // is divided evenly among the renderers.
  void AddToStrategy(const std::set<int>& renderers,
                     AllocationTactic tactic,
                     uint64_t extra_bytes_to_allocate,
                     AllocationStrategy* strategy);

  // Enact an allocation strategy by informing the renderers of their
  // allocations according to |strategy|.
  void EnactStrategy(const AllocationStrategy& strategy);

  enum ClearCacheOccasion {
    // Instructs to clear the cache instantly.
    INSTANTLY,
    // Instructs to clear the cache when a navigation takes place (this
    // includes reloading a tab).
    ON_NAVIGATION
  };

  // Inform all |renderers| to clear their cache.
  void ClearRendererCache(const std::set<int>& renderers,
                          ClearCacheOccasion occation);

  // Check to see if any active renderers have fallen inactive.
  void FindInactiveRenderers();

  // The global size limit for all in-memory caches.
  uint64_t global_size_limit_;

  // Maps every renderer_id our most recent copy of its statistics.
  StatsMap stats_;

  // Every renderer we think is still around is in one of these two sets.
  //
  // Active renderers are those renderers that have been active more recently
  // than they have been inactive.
  std::set<int> active_renderers_;
  // Inactive renderers are those renderers that have been inactive more
  // recently than they have been active.
  std::set<int> inactive_renderers_;

  // Maps every renderer_id with its corresponding
  // mojo::Remote<mojom::WebCache>.
  WebCacheServicesMap web_cache_services_;

  ScopedObserver<content::RenderProcessHost, content::RenderProcessHostObserver>
      rph_observers_{this};

  base::WeakPtrFactory<WebCacheManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebCacheManager);
};

}  // namespace web_cache

#endif  // COMPONENTS_WEB_CACHE_BROWSER_WEB_CACHE_MANAGER_H_
