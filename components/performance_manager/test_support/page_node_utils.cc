// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/page_node_utils.h"

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager::testing {

PageNode* GetPageNodeForWebContents(content::WebContents* contents) {
  CHECK(PerformanceManager::IsAvailable());
  return PerformanceManager::GetPrimaryPageNodeForWebContents(contents).get();
}

void SetPageNodeType(PageNode* page_node, PageType page_type) {
  PageNodeImpl::FromNode(page_node)->SetType(page_type);
}

void SetPageNodeLoadingState(PageNode* page_node,
                             PageNode::LoadingState state) {
  PageNodeImpl::FromNode(page_node)->SetLoadingState(state);
}

}  // namespace performance_manager::testing
