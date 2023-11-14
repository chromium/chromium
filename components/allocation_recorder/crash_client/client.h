// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ALLOCATION_RECORDER_CRASH_CLIENT_CLIENT_H_
#define COMPONENTS_ALLOCATION_RECORDER_CRASH_CLIENT_CLIENT_H_

namespace base::debug::tracer {
class AllocationTraceRecorder;
}

namespace allocation_recorder::crash_client {

// Register the given recorder with the client part of the allocation stack
// recorder's Crashpad integration. This creates the required structures etc. to
// properly communicate the data of the passed recorder to the handler part.
// Only one recorder can be registered at a time.
//
// |RegisterRecorderWithCrashpad| terminates the processes if any recorder is
// already registered (see |UnregisterRecorderWithCrashpad| to unregister the
// current recorder).
void RegisterRecorderWithCrashpad(
    base::debug::tracer::AllocationTraceRecorder& recorder);

// Unregister the currently registered recorder. This sets the size of
// annotation to 0, which effectively disables it. Note that the annotation is
// still present as it can't be deleted (Crashpad limitation).
//
// |UnregisterRecorderWithCrashpad| terminates the processes if called without
// registering a recorder first.
void UnregisterRecorderWithCrashpad();

}  // namespace allocation_recorder::crash_client

#endif  // COMPONENTS_ALLOCATION_RECORDER_CRASH_CLIENT_CLIENT_H_
