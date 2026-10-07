// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_APP_SHARED_MEMORY_USER_STREAM_MACH_PORT_H_
#define COMPONENTS_CRASH_CORE_APP_SHARED_MEMORY_USER_STREAM_MACH_PORT_H_

#include <vector>

#include "base/apple/scoped_mach_port.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "build/build_config.h"

static_assert(BUILDFLAG(IS_MAC));

namespace crash_reporter::internal {

// Creates a new Mach port and sends `user_streams` to the port. Returns the
// receive right for the new port, to be passed to Crashpad. This function runs
// in Chrome.
base::apple::ScopedMachReceiveRight SendSharedMemoryUserStreamsToPort(
    std::vector<base::ReadOnlySharedMemoryRegion> user_streams);

// Receives shared-memory user stream regions from the given Mach `port` and
// returns them. This function runs in the Crashpad process.
std::vector<base::ReadOnlySharedMemoryRegion>
ReceiveSharedMemoryUserStreamsFromPort(
    base::apple::ScopedMachReceiveRight port);

}  // namespace crash_reporter::internal

#endif  // COMPONENTS_CRASH_CORE_APP_SHARED_MEMORY_USER_STREAM_MACH_PORT_H_
