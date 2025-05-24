// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_UNSAFE_RESOURCE_LOCATOR_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_UNSAFE_RESOURCE_LOCATOR_H_

#include <optional>

#include "base/unguessable_token.h"

namespace security_interstitials {

// Contains indentifiers which globally identify a RenderFrameHost.
// UnsafeResourceLocator is associated with a valid UnsafeResource.
struct UnsafeResourceLocator {
  // TODO(https://crbug.com/40686246): These are content/ specific types that
  // are used in UnsafeResourceLocator, in violation of layering. Refactor and
  // remove them.
  //
  // TODO(https://crbug.com/40683815): Note that components/safe_browsing relies
  // on this violation of layering to implement its own layering violation, so
  // that issue might need to be fixed first.

  // Equivalent to GlobalRenderFrameHostId.
  using RenderProcessId = int;
  using RenderFrameToken = std::optional<base::UnguessableToken>;
  // This is the underlying value type of content::FrameTreeNodeId.
  using FrameTreeNodeId = int;

  // Copies of the sentinel values used in content/.
  // Equal to ChildProcessHost::kInvalidUniqueID.
  static constexpr RenderProcessId kNoRenderProcessId = -1;
  // Equal to the invalid value of content::FrameTreeNodeId.
  static constexpr FrameTreeNodeId kNoFrameTreeNodeId = -1;

  UnsafeResourceLocator();
  ~UnsafeResourceLocator();

  static UnsafeResourceLocator CreateForRenderFrameToken(
      RenderProcessId render_process_id,
      RenderFrameToken render_frame_token);

  static UnsafeResourceLocator CreateForFrameTreeNodeId(
      FrameTreeNodeId frame_tree_node_id);

  UnsafeResourceLocator(RenderProcessId render_process_id,
                        const RenderFrameToken& render_frame_token,
                        FrameTreeNodeId frame_tree_node_id);

  UnsafeResourceLocator(const UnsafeResourceLocator&);
  UnsafeResourceLocator& operator=(const UnsafeResourceLocator&);

  // These content/ specific ids indicate what triggered safe browsing. In the
  // case of a frame navigating, we should have its FrameTreeNode id and
  // navigation id. In the case of something triggered by a document (e.g.
  // subresource loading), we should have the RenderFrameHost's id.
  RenderProcessId render_process_id = kNoRenderProcessId;
  RenderFrameToken render_frame_token;
  FrameTreeNodeId frame_tree_node_id = kNoFrameTreeNodeId;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_UNSAFE_RESOURCE_LOCATOR_H_
