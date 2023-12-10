// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_CRONET_GLOBAL_STATE_H_
#define COMPONENTS_CRONET_CRONET_GLOBAL_STATE_H_

#include <memory>
#include <string>
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

namespace net {
class NetLog;
class ProxyConfigService;
class ProxyResolutionService;
}  // namespace net

namespace cronet {

// Returns true when called on the initialization thread.
// May only be called after EnsureInitialized() has returned.
bool OnInitThread();

// Posts a task to run on initialization thread. Blocks until initialization
// thread is started.
void PostTaskToInitThread(const base::Location& posted_from,
                          base::OnceClosure task);

// Performs one-off initialization of Cronet global state, including creating,
// or binding to an existing thread, to run initialization and process
// network notifications on. The implementation must be thread-safe and
// idempotent, and must complete initialization before returning.
void EnsureInitialized();

// Creates a proxy config service appropriate for this platform that fetches the
// system proxy settings. Cronet will call this API only after a prior call
// to EnsureInitialized() has returned.
// On Android, this must be called on the JNI thread.
std::unique_ptr<net::ProxyConfigService> CreateProxyConfigService(
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);

// Creates a proxy resolution service appropriate for this platform that fetches
// the system proxy settings. Cronet will call this API only after a prior call
// to EnsureInitialized() has returned.
std::unique_ptr<net::ProxyResolutionService> CreateProxyResolutionService(
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    net::NetLog* net_log);

// Creates default User-Agent request value, combining optional
// |partial_user_agent| with system-dependent values. This API may be invoked
// before EnsureInitialized(), in which case it may trigger initialization
// itself, if necessary.
std::string CreateDefaultUserAgent(const std::string& partial_user_agent);

// Set network thread priority to |priority|. Must be called on the network
// thread. Corresponds to android.os.Process.setThreadPriority()
// values.
void SetNetworkThreadPriorityOnNetworkThread(double priority);

}  // namespace cronet

#endif  // COMPONENTS_CRONET_CRONET_GLOBAL_STATE_H_
