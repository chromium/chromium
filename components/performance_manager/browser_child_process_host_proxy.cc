// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/browser_child_process_host_proxy.h"

#include "content/public/browser/browser_child_process_host.h"

namespace performance_manager {

BrowserChildProcessHostProxy::BrowserChildProcessHostProxy() = default;
BrowserChildProcessHostProxy::~BrowserChildProcessHostProxy() = default;
BrowserChildProcessHostProxy::BrowserChildProcessHostProxy(
    const BrowserChildProcessHostProxy& other) = default;
BrowserChildProcessHostProxy& BrowserChildProcessHostProxy::operator=(
    const BrowserChildProcessHostProxy& other) = default;

content::BrowserChildProcessHost* BrowserChildProcessHostProxy::Get() const {
  return content::BrowserChildProcessHost::FromID(
      browser_child_process_host_id_.value());
}

BrowserChildProcessHostProxy::BrowserChildProcessHostProxy(
    BrowserChildProcessHostId browser_child_process_host_id)
    : browser_child_process_host_id_(browser_child_process_host_id) {
  DCHECK_GE(browser_child_process_host_id.value(), 0);
}

// static
BrowserChildProcessHostProxy BrowserChildProcessHostProxy::CreateForTesting(
    BrowserChildProcessHostId browser_child_process_host_id) {
  return BrowserChildProcessHostProxy(browser_child_process_host_id);
}

}  // namespace performance_manager
