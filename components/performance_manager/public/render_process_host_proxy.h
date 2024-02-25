// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_PROXY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_PROXY_H_

#include "components/performance_manager/public/render_process_host_id.h"
#include "content/public/browser/child_process_host.h"

namespace content {
class RenderProcessHost;
}  // namespace content

namespace performance_manager {

// A RenderProcessHostProxy is used to post messages out of the performance
// manager sequence that are bound for a RenderProcessHost running on the UI
// thread. The object is bound to the UI thread. A RenderProcessHostProxy is
// conceputally equivalent to a WeakPtr<RenderProcessHost>. Copy and assignment
// are explicitly allowed for this object.
class RenderProcessHostProxy {
 public:
  RenderProcessHostProxy();
  RenderProcessHostProxy(const RenderProcessHostProxy& other);
  ~RenderProcessHostProxy();
  RenderProcessHostProxy& operator=(const RenderProcessHostProxy& other);

  // Allows resolving this proxy to the underlying RenderProcessHost. This must
  // only be called on the UI thread. Returns nullptr if the RenderProcessHost
  // no longer exists.
  content::RenderProcessHost* Get() const;

  // Returns true iff the proxy has a valid RenderProcessHostId (not 0 or
  // ChildProcessHost::kInvalidUniqueId).
  bool is_valid() const { return !render_process_host_id_.is_null(); }

  // Returns the routing id of the render process (from
  // RenderProcessHost::GetID).
  RenderProcessHostId render_process_host_id() const {
    return render_process_host_id_;
  }

  static RenderProcessHostProxy CreateForTesting(
      RenderProcessHostId render_process_host_id);

 protected:
  friend class RenderProcessUserData;

  explicit RenderProcessHostProxy(RenderProcessHostId render_process_host_id);

 private:
  RenderProcessHostId render_process_host_id_ =
      RenderProcessHostId(content::ChildProcessHost::kInvalidUniqueID);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_PROXY_H_
