// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PAGE_NODE_UTILS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PAGE_NODE_UTILS_H_

#include "components/performance_manager/public/graph/page_node.h"

namespace content {
class WebContents;
}

namespace performance_manager::testing {

// Returns a PageNode for the given WebContents, which should never be null.
// Asserts that Performance Manager is initialized.
PageNode* GetPageNodeForWebContents(content::WebContents* contents);

// Sets the type of a PageNode. In production this is set to kTab when the
// node is added to a tab strip, which isn't always available in unit tests.
void SetPageNodeType(PageNode* page_node, PageType page_type);

// Updates a PageNode's loading state. In production this is driven by
// PageLoadTrackerDecorator, but in many unit tests it's easier to test state
// transitions by updating the state directly.
void SetPageNodeLoadingState(PageNode* page_node, PageNode::LoadingState state);

}  // namespace performance_manager::testing

#endif  // COMPONENTS_PERFORMANCE_MANAGER_TEST_SUPPORT_PAGE_NODE_UTILS_H_
