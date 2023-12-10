// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_BROWSER_CHILD_PROCESS_HOST_PROXY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_BROWSER_CHILD_PROCESS_HOST_PROXY_H_

#include "components/performance_manager/public/browser_child_process_host_id.h"
#include "content/public/browser/child_process_host.h"

namespace content {
class BrowserChildProcessHost;
}  // namespace content

namespace performance_manager {

// A BrowserChildProcessHostProxy is used to post messages out of the
// performance manager sequence that are bound for a BrowserChildProcessHost
// running on the UI thread. The object is bound to the UI thread. A
// BrowserChildProcessHostProxy is conceputally equivalent to a
// WeakPtr<BrowserChildProcessHost>. Copy and assignment are explicitly allowed
// for this object.
class BrowserChildProcessHostProxy {
 public:
  BrowserChildProcessHostProxy();
  BrowserChildProcessHostProxy(const BrowserChildProcessHostProxy& other);
  ~BrowserChildProcessHostProxy();
  BrowserChildProcessHostProxy& operator=(
      const BrowserChildProcessHostProxy& other);

  // Allows resolving this proxy to the underlying BrowserChildProcessHost. This
  // must only be called on the UI thread. Returns nullptr if the
  // BrowserChildProcessHost no longer exists.
  content::BrowserChildProcessHost* Get() const;

  // Returns true iff the proxy has a valid BrowserChildProcessHostId (not 0 or
  // ChildProcessHost::kInvalidUniqueId).
  bool is_valid() const { return !browser_child_process_host_id_.is_null(); }

  // Returns the routing id of the BrowserChildProcessHost (from
  // BrowserChildProcessHost::GetID).
  BrowserChildProcessHostId browser_child_process_host_id() const {
    return browser_child_process_host_id_;
  }

  static BrowserChildProcessHostProxy CreateForTesting(
      BrowserChildProcessHostId browser_child_process_host_id);

 protected:
  friend class BrowserChildProcessWatcher;

  explicit BrowserChildProcessHostProxy(
      BrowserChildProcessHostId browser_child_process_host_id);

 private:
  BrowserChildProcessHostId browser_child_process_host_id_ =
      BrowserChildProcessHostId(content::ChildProcessHost::kInvalidUniqueID);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_BROWSER_CHILD_PROCESS_HOST_PROXY_H_
