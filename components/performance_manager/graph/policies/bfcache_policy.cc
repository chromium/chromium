// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/policies/bfcache_policy.h"

#include "base/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager::policies {

namespace {

// Whether or not the BFCache of all pages should be flushed when the system
// is under *moderate* memory pressure. The policy always flushes the bfcache
// under critical pressure.
bool IsFlushOnModeratePressureEnabled() {
  static constexpr base::FeatureParam<bool> flush_on_moderate_pressure{
      &features::kBFCachePerformanceManagerPolicy, "flush_on_moderate_pressure",
      false};

  return flush_on_moderate_pressure.Get();
}

bool PageMightHaveFramesInBFCache(const PageNode* page_node) {
  // TODO(crbug.com/1211368): Use PageState when that actually works.
  auto main_frame_nodes = page_node->GetMainFrameNodes();
  if (main_frame_nodes.size() == 1)
    return false;
  for (const auto* main_frame_node : main_frame_nodes) {
    if (!main_frame_node->IsCurrent())
      return true;
  }
  return false;
}

void MaybeFlushBFCacheOnUIThread(const WebContentsProxy& contents_proxy) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* const content = contents_proxy.Get();
  if (!content)
    return;

  // Do not flush the BFCache if there's a pending navigation as this could stop
  // it.
  // TODO(sebmarchand): Check if this is really needed.
  auto& navigation_controller = content->GetController();
  if (!navigation_controller.GetPendingEntry())
    navigation_controller.GetBackForwardCache().Flush();
}

}  // namespace

BFCachePolicy::BFCachePolicy()
    : flush_on_moderate_pressure_{IsFlushOnModeratePressureEnabled()} {}

BFCachePolicy::~BFCachePolicy() = default;

void BFCachePolicy::MaybeFlushBFCache(const PageNode* page_node) {
  DCHECK(page_node);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MaybeFlushBFCacheOnUIThread,
                                page_node->GetContentsProxy()));
}

void BFCachePolicy::OnPassedToGraph(Graph* graph) {
  DCHECK(graph->HasOnlySystemNode());
  graph_ = graph;
  graph_->AddSystemNodeObserver(this);
}

void BFCachePolicy::OnTakenFromGraph(Graph* graph) {
  graph_->RemoveSystemNodeObserver(this);
  graph_ = nullptr;
}

void BFCachePolicy::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel new_level) {
  // This shouldn't happen but add the check anyway in case the API changes.
  if (new_level == base::MemoryPressureListener::MemoryPressureLevel::
                       MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  if (new_level == base::MemoryPressureListener::MemoryPressureLevel::
                       MEMORY_PRESSURE_LEVEL_MODERATE &&
      !flush_on_moderate_pressure_) {
    return;
  }

  // Flush the cache of all pages.
  for (auto* page_node : graph_->GetAllPageNodes()) {
    if (page_node->GetPageState() == PageNode::PageState::kActive &&
        PageMightHaveFramesInBFCache(page_node)) {
      MaybeFlushBFCache(page_node);
    }
  }
}

}  // namespace performance_manager::policies
