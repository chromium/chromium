// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_TRACING_SYSTEM_TRACING_COMMON_H_
#define CHROMECAST_TRACING_SYSTEM_TRACING_COMMON_H_

#include <sys/socket.h>
#include <sys/un.h>

#include <array>

#include "base/files/scoped_file.h"
#include "base/trace_event/common/trace_event_common.h"

namespace chromecast {
namespace tracing {

inline constexpr std::array<const char*, 6> kCategories = {
    "gfx",   "input", TRACE_DISABLED_BY_DEFAULT("irq"),
    "power", "sched", "workq"};

sockaddr_un GetSystemTracingSocketAddress();

// Creates the listening socket for the system tracing service, bound to
// |addr.sun_path|. Any pre-existing node at that path is removed first.
//
// The node is created with mode 0660. The service runs with the privileges
// required to drive system-wide ftrace, so any process that can connect can
// enable kernel tracing and read the trace buffer; a world-accessible node
// would hand that capability to every local process. Creation fails rather
// than falling back to a permissive mode.
//
// Returns an invalid fd on failure.
base::ScopedFD CreateTracingServerSocket(const sockaddr_un& addr);

// Returns whether the peer connected on |socket_fd| is allowed to use the
// tracing service. Only peers running as the same user as the service are
// accepted.
//
// This is deliberately redundant with the socket mode: the mode is applied to
// a path under a directory this process does not own, so it can be widened by
// a mis-configured deployment, whereas SO_PEERCRED is supplied by the kernel
// and cannot be spoofed by the client.
bool IsTracingClientAuthorized(int socket_fd);

}  // namespace tracing
}  // namespace chromecast

#endif  // CHROMECAST_TRACING_SYSTEM_TRACING_COMMON_H_
