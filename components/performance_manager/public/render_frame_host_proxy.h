// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_FRAME_HOST_PROXY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_FRAME_HOST_PROXY_H_

#include "content/public/browser/global_routing_id.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace performance_manager {

class FrameNodeImpl;

// A RenderFrameHostProxy is used to post messages out of the performance
// manager sequence that are bound for a RenderFrameHost running on the UI
// thread. The object is bound to the UI thread. A RenderFrameHostProxy is
// conceptually equivalent to a WeakPtr<RenderFrameHost>. Copy and assignment
// are explicitly allowed for this object.
class RenderFrameHostProxy {
 public:
  RenderFrameHostProxy();
  RenderFrameHostProxy(const RenderFrameHostProxy& other);
  ~RenderFrameHostProxy();
  RenderFrameHostProxy& operator=(const RenderFrameHostProxy& other);

  // Allows resolving this proxy to the underlying RenderFrameHost. This must
  // only be called on the UI thread. Returns nullptr if the RenderFrameHost
  // no longer exists.
  content::RenderFrameHost* Get() const;

  // Returns true iff the proxy has a valid GlobalRenderFrameHostId (whose
  // operator::bool returns true).
  bool is_valid() const { return static_cast<bool>(global_frame_routing_id_); }

  // Returns the global routing ID.
  const content::GlobalRenderFrameHostId& global_frame_routing_id() const {
    return global_frame_routing_id_;
  }

 protected:
  friend class FrameNodeImpl;

  explicit RenderFrameHostProxy(
      const content::GlobalRenderFrameHostId& global_frame_routing_id);

 private:
  content::GlobalRenderFrameHostId global_frame_routing_id_;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_FRAME_HOST_PROXY_H_
