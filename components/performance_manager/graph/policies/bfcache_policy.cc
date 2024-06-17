// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/policies/bfcache_policy.h"

#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager::policies {

namespace {

// The foregrounded tab's cache limit on moderate memory pressure. The negative
// value means no limit.
int ForegroundCacheSizeOnModeratePressure() {
  static constexpr base::FeatureParam<int>
      foreground_cache_size_on_moderate_pressure{
          &features::kBFCachePerformanceManagerPolicy,
          "foreground_cache_size_on_moderate_pressure", 3};
  return foreground_cache_size_on_moderate_pressure.Get();
}

// The backgrounded tab's cache limit on moderate memory pressure. The negative
// value means no limit.
int BackgroundCacheSizeOnModeratePressure() {
  static constexpr base::FeatureParam<int>
      background_cache_size_on_moderate_pressure{
          &features::kBFCachePerformanceManagerPolicy,
          "background_cache_size_on_moderate_pressure", 1};
  return background_cache_size_on_moderate_pressure.Get();
}

// The foregrounded tab's cache limit on critical memory pressure. The negative
// value means no limit.
int ForegroundCacheSizeOnCriticalPressure() {
  static constexpr base::FeatureParam<int>
      foreground_cache_size_on_critical_pressure{
          &features::kBFCachePerformanceManagerPolicy,
          "foreground_cache_size_on_critical_pressure", 0};
  return foreground_cache_size_on_critical_pressure.Get();
}

// The backgrounded tab's cache limit on critical memory pressure. The negative
// value means no limit.
int BackgroundCacheSizeOnCriticalPressure() {
  static constexpr base::FeatureParam<int>
      background_cache_size_on_critical_pressure{
          &features::kBFCachePerformanceManagerPolicy,
          "background_cache_size_on_critical_pressure", 0};
  return background_cache_size_on_critical_pressure.Get();
}

bool PageMightHaveFramesInBFCache(const PageNode* page_node) {
  // TODO(crbug.com/40182881): Use PageState when that actually works.
  auto main_frame_nodes = page_node->GetMainFrameNodes();
  if (main_frame_nodes.size() == 1)
    return false;
  for (const FrameNode* main_frame_node : main_frame_nodes) {
    if (!main_frame_node->IsCurrent())
      return true;
  }
  return false;
}

using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

void MaybeFlushBFCacheOnUIThread(base::WeakPtr<content::WebContents> contents,
                                 MemoryPressureLevel memory_pressure_level) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!contents) {
    return;
  }

  int cache_size = -1;
  bool foregrounded =
      (contents->GetVisibility() == content::Visibility::VISIBLE);
  switch (memory_pressure_level) {
    case MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE:
      cache_size = foregrounded ? ForegroundCacheSizeOnModeratePressure()
                                : BackgroundCacheSizeOnModeratePressure();
      break;
    case MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL:
      cache_size = foregrounded ? ForegroundCacheSizeOnCriticalPressure()
                                : BackgroundCacheSizeOnCriticalPressure();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  // Do not flush BFCache if cache_size is negative (such as -1).
  if (cache_size < 0)
    return;

  // Do not flush the BFCache if there's a pending navigation as this could stop
  // it.
  // TODO(sebmarchand): Check if this is really needed.
  auto& navigation_controller = contents->GetController();
  if (!navigation_controller.GetPendingEntry())
    navigation_controller.GetBackForwardCache().Prune(cache_size);
}

}  // namespace

void BFCachePolicy::MaybeFlushBFCache(
    const PageNode* page_node,
    MemoryPressureLevel memory_pressure_level) {
  DCHECK(page_node);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MaybeFlushBFCacheOnUIThread, page_node->GetWebContents(),
                     memory_pressure_level));
}

void BFCachePolicy::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->HasOnlySystemNode());
  graph->AddSystemNodeObserver(this);
}

void BFCachePolicy::OnTakenFromGraph(Graph* graph) {
  graph->RemoveSystemNodeObserver(this);
}

void BFCachePolicy::OnMemoryPressure(MemoryPressureLevel new_level) {
  // This shouldn't happen but add the check anyway in case the API changes.
  if (new_level == MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  // Apply the cache limit to all pages.
  for (const PageNode* page_node : GetOwningGraph()->GetAllPageNodes()) {
    if (PageMightHaveFramesInBFCache(page_node)) {
      MaybeFlushBFCache(page_node, new_level);
    }
  }
}

}  // namespace performance_manager::policies
