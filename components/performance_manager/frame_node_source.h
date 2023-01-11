// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_FRAME_NODE_SOURCE_H_
#define COMPONENTS_PERFORMANCE_MANAGER_FRAME_NODE_SOURCE_H_

#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "content/public/browser/global_routing_id.h"

namespace performance_manager {

class FrameNodeImpl;

// Represents a source of existing frame nodes that lives on the main thread.
// In practice, this is used by the worker watchers as an abstraction over the
// PerformanceManagerTabHelper to make testing easier.
class FrameNodeSource {
 public:
  virtual ~FrameNodeSource() = default;

  using OnbeforeFrameNodeRemovedCallback =
      base::OnceCallback<void(FrameNodeImpl*)>;

  // Returns the frame node associated with |render_process_host_id|. Returns
  // null if no such node exists.
  virtual FrameNodeImpl* GetFrameNode(
      content::GlobalRenderFrameHostId render_process_host_id) = 0;

  // Subscribes to receive removal notification for a frame node.
  virtual void SubscribeToFrameNode(
      content::GlobalRenderFrameHostId render_process_host_id,
      OnbeforeFrameNodeRemovedCallback
          on_before_frame_node_removed_callback) = 0;

  // Unsubscribes to a frame node
  virtual void UnsubscribeFromFrameNode(
      content::GlobalRenderFrameHostId render_process_host_id) = 0;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_FRAME_NODE_SOURCE_H_
