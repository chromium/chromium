// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_PAGE_CONTEXT_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_PAGE_CONTEXT_H_

#include <compare>
#include <string>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/performance_manager/public/web_contents_proxy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebContents;
}

namespace performance_manager {
class PageNode;
}

namespace performance_manager::resource_attribution {

class PageContext {
 public:
  ~PageContext();

  PageContext(const PageContext& other);
  PageContext& operator=(const PageContext& other);
  PageContext(PageContext&& other);
  PageContext& operator=(PageContext&& other);

  // UI thread methods

  // Returns the PageContext for `contents`, which must be non-null. Returns
  // nullopt if the WebContents is not registered with PerformanceManager.
  static absl::optional<PageContext> FromWebContents(
      content::WebContents* contents);

  // Returns the WebContents for this context, or nullptr if it no longer
  // exists.
  content::WebContents* GetWebContents() const;

  // Returns the PageNode for this context, or a null WeakPtr if it no longer
  // exists.
  base::WeakPtr<PageNode> GetWeakPageNode() const;

  // PM sequence methods

  // Returns the PageContext for `node`. Equivalent to
  // node->GetResourceContext().
  static PageContext FromPageNode(const PageNode* node);

  // Returns the PageContext for `node`, or nullopt if `node` is null.
  static absl::optional<PageContext> FromWeakPageNode(
      base::WeakPtr<PageNode> node);

  // Returns the PageNode for this context, or nullptr if it no longer exists.
  PageNode* GetPageNode() const;

  // Returns a string representation of the context for debugging. This matches
  // the interface of base::TokenType and base::UnguessableToken, for
  // convenience.
  std::string ToString() const;

  // Compare PageContexts by PageNode token.
  friend auto operator<=>(const PageContext& a, const PageContext& b) {
    return a.token_ <=> b.token_;
  }

  // Test PageContexts for equality by PageNode token.
  friend bool operator==(const PageContext& a, const PageContext& b) {
    return a.token_ == b.token_;
  }

 private:
  PageContext(base::UnguessableToken token,
              WebContentsProxy web_contents_proxy,
              base::WeakPtr<PageNode> weak_node);

  // A unique identifier for the PageNode. A PageNodeImpl::PageToken will be
  // assigned to this, but DEPS rules won't let PageNodeImpl be included in a
  // public header so UnguessableToken is used directly here. Used to ensure
  // that PageContexts for the same PageNode compare equal.
  base::UnguessableToken token_;

  // A PerformanceManager proxy object that resolves to the WebContents on the
  // UI thread.
  WebContentsProxy web_contents_proxy_;

  // A pointer to the PageNode that must be dereferenced on the PM sequence.
  base::WeakPtr<PageNode> weak_node_;
};

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_PAGE_CONTEXT_H_
