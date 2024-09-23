// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_TRACING_SYSTEM_TRACING_COMMON_H_
#define CHROMECAST_TRACING_SYSTEM_TRACING_COMMON_H_

#include <sys/socket.h>
#include <sys/un.h>

#include "base/trace_event/common/trace_event_common.h"

namespace chromecast {
namespace tracing {

inline constexpr std::array<const char*, 6> kCategories = {
    "gfx",   "input", TRACE_DISABLED_BY_DEFAULT("irq"),
    "power", "sched", "workq"};

sockaddr_un GetSystemTracingSocketAddress();

}  // namespace tracing
}  // namespace chromecast

#endif  // CHROMECAST_TRACING_SYSTEM_TRACING_COMMON_H_
