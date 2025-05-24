// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/core/unsafe_resource_locator.h"

namespace security_interstitials {

UnsafeResourceLocator::UnsafeResourceLocator() = default;
UnsafeResourceLocator::~UnsafeResourceLocator() = default;

// static
UnsafeResourceLocator UnsafeResourceLocator::CreateForRenderFrameToken(
    RenderProcessId render_process_id,
    RenderFrameToken render_frame_token) {
  return UnsafeResourceLocator(render_process_id, render_frame_token,
                               kNoFrameTreeNodeId);
}

// static
UnsafeResourceLocator UnsafeResourceLocator::CreateForFrameTreeNodeId(
    FrameTreeNodeId frame_tree_node_id) {
  return UnsafeResourceLocator(kNoRenderProcessId,
                               /*render_frame_token=*/std::nullopt,
                               frame_tree_node_id);
}

UnsafeResourceLocator::UnsafeResourceLocator(
    RenderProcessId render_process_id,
    const RenderFrameToken& render_frame_token,
    FrameTreeNodeId frame_tree_node_id)
    : render_process_id(render_process_id),
      render_frame_token(render_frame_token),
      frame_tree_node_id(frame_tree_node_id) {}

UnsafeResourceLocator::UnsafeResourceLocator(const UnsafeResourceLocator&) =
    default;
UnsafeResourceLocator& UnsafeResourceLocator::operator=(
    const UnsafeResourceLocator&) = default;

}  // namespace security_interstitials
