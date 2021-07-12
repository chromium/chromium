// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/graph/policies/bfcache_policy.h"

#include "base/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/task/task_traits.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {
namespace policies {

namespace {

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
  auto& navigation_controller = content->GetController();
  if (!navigation_controller.GetPendingEntry())
    navigation_controller.GetBackForwardCache().Flush();
}

}  // namespace

BFCachePolicy::BFCachePolicy() = default;

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
  graph_->AddPageNodeObserver(this);
  graph_->AddSystemNodeObserver(this);
}

void BFCachePolicy::OnTakenFromGraph(Graph* graph) {
  graph_->RemovePageNodeObserver(this);
  graph_->RemoveSystemNodeObserver(this);
  graph_ = nullptr;
}

void BFCachePolicy::OnIsVisibleChanged(const PageNode* page_node) {
  // Try to flush the BFCache of pages when they become non-visible. This could
  // fail if the page still has a pending navigation.
  if (page_node->GetPageState() == PageState::kActive &&
      !page_node->IsVisible() && PageMightHaveFramesInBFCache(page_node)) {
    MaybeFlushBFCache(page_node);
  }
}

void BFCachePolicy::OnLoadingStateChanged(const PageNode* page_node) {
  // Flush the BFCache of pages that finish a navigation while in background.
  if (!page_node->IsVisible() &&
      page_node->GetLoadingState() >= PageNode::LoadingState::kLoadedBusy &&
      PageMightHaveFramesInBFCache(page_node)) {
    MaybeFlushBFCache(page_node);
  }
}

void BFCachePolicy::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel new_level) {
  // This shouldn't happen but add the check anyway in case the API changes.
  if (new_level == base::MemoryPressureListener::MemoryPressureLevel::
                       MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  if (new_level == base::MemoryPressureListener::MemoryPressureLevel::
                       MEMORY_PRESSURE_LEVEL_NONE &&
      !performance_manager::features::BFCachePerformanceManagerPolicyParams::
           GetParams()
               .flush_on_moderate_pressure()) {
    return;
  }

  // Flush the cache of all visible pages, the bfcache of background pages
  // should already be empty.
  for (auto* page_node : graph_->GetAllPageNodes()) {
    if (page_node->GetPageState() == PageState::kActive &&
        page_node->IsVisible() && PageMightHaveFramesInBFCache(page_node)) {
      MaybeFlushBFCache(page_node);
    }
  }
}

}  // namespace policies
}  // namespace performance_manager
