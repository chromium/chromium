// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ALLOCATION_RECORDER_CRASH_CLIENT_CLIENT_H_
#define COMPONENTS_ALLOCATION_RECORDER_CRASH_CLIENT_CLIENT_H_

namespace base::debug::tracer {
struct AllocationTraceRecorder;
}

namespace allocation_recorder::crash_client {
// Initialize the client part of the allocation stack recorder's crashpad
// integration. This creates the required structures etc. to properly
// communicate the data of the passed recorder to the handler part.
//
// Returns a reference to the allocation trace recorder which will be included
// in the crash report.
base::debug::tracer::AllocationTraceRecorder& Initialize();
}  // namespace allocation_recorder::crash_client

#endif  // COMPONENTS_ALLOCATION_RECORDER_CRASH_CLIENT_CLIENT_H_
