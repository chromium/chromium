// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_FRAME_CONTEXT_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_FRAME_CONTEXT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class RenderFrameHost;
}

namespace performance_manager {
class FrameNode;
}

namespace performance_manager::resource_attribution {

class FrameContext {
 public:
  ~FrameContext();

  FrameContext(const FrameContext& other);
  FrameContext& operator=(const FrameContext& other);
  FrameContext(FrameContext&& other);
  FrameContext& operator=(FrameContext&& other);

  // UI thread methods.

  // Returns the FrameContext for `host`, which must be non-null and have a
  // valid GlobalRenderFrameHostId. Returns nullopt if the RenderFrameHost is
  // not registered with PerformanceManager. (There is a brief window after the
  // RenderFrameHost is created before a PerformanceManager FrameNode is created
  // for it.)
  static absl::optional<FrameContext> FromRenderFrameHost(
      content::RenderFrameHost* host);

  // Returns the RenderFrameHost for this context, or nullptr if it no longer
  // exists.
  content::RenderFrameHost* GetRenderFrameHost() const;

  // Return the GlobalRenderFrameHostId that was assigned to this context's
  // RenderFrameHost.
  content::GlobalRenderFrameHostId GetRenderFrameHostId() const;

  // Returns the FrameNode for this context, or a null WeakPtr if it no longer
  // exists.
  base::WeakPtr<FrameNode> GetWeakFrameNode() const;

  // PM sequence methods.

  // Returns the FrameContext for `node`. Equivalent to
  // node->GetResourceContext().
  static FrameContext FromFrameNode(const FrameNode* node);

  // Returns the FrameContext for `node`, or nullopt if `node` is null.
  static absl::optional<FrameContext> FromWeakFrameNode(
      base::WeakPtr<FrameNode> node);

  // Returns the FrameNode for this context, or nullptr if it no longer exists.
  FrameNode* GetFrameNode() const;

  // Returns a string representation of the context for debugging. This matches
  // the interface of base::TokenType and base::UnguessableToken, for
  // convenience.
  std::string ToString() const;

 private:
  friend bool operator<(const FrameContext&, const FrameContext&);
  friend bool operator==(const FrameContext&, const FrameContext&);

  FrameContext(content::GlobalRenderFrameHostId id,
               base::WeakPtr<FrameNode> weak_node);

  content::GlobalRenderFrameHostId id_;
  base::WeakPtr<FrameNode> weak_node_;
};

inline bool operator<(const FrameContext& a, const FrameContext& b) {
  return a.id_ < b.id_;
}

inline bool operator==(const FrameContext& a, const FrameContext& b) {
  return a.id_ == b.id_;
}

inline bool operator!=(const FrameContext& a, const FrameContext& b) {
  return !(a == b);
}

inline bool operator<=(const FrameContext& a, const FrameContext& b) {
  return !(b < a);
}

inline bool operator>(const FrameContext& a, const FrameContext& b) {
  return !(a < b || a == b);
}

inline bool operator>=(const FrameContext& a, const FrameContext& b) {
  return !(a > b);
}

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_FRAME_CONTEXT_H_
