// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ALLOCATION_RECORDER_TESTING_CRASH_VERIFICATION_H_
#define COMPONENTS_ALLOCATION_RECORDER_TESTING_CRASH_VERIFICATION_H_

#include "base/debug/debugging_buildflags.h"
#include "base/functional/callback.h"

namespace allocation_recorder {
class Payload;
}

namespace base {
class FilePath;
}

namespace allocation_recorder::testing {

// Verify that a crash creates a Crashpad report and that this report complies
// to some test specific requirements. |crash_function| is required to be set
// and to cause a single Crashpad report to be created.

// Crash a process and verify that the Crashpad report has no data from the
// AllocationTraceRecorder.
void VerifyCrashCreatesCrashpadReportWithoutAllocationRecorderStream(
    const base::FilePath& crashpad_database_path,
    base::OnceClosure crash_function);

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
// Crash a process and verify that the Crashpad report:
// - contains exactly one data stream from the AllocationTraceRecorder.
// - this data stream's payload can be deserialized into the
// AllocationTraceRecorder's payload.
// - if set, check that the deserialized payload is accepted by
// |payload_verification|.
void VerifyCrashCreatesCrashpadReportWithAllocationRecorderStream(
    const base::FilePath& crashpad_database_path,
    base::OnceClosure crash_function,
    base::OnceCallback<void(const allocation_recorder::Payload& payload)>
        payload_verification);

// Verify some basic properties of the payload are set. If
// |expect_report_with_content| is true, at least one allocation entry is
// required, otherwise there must be no entries.
void VerifyPayload(const bool expect_report_with_content,
                   const allocation_recorder::Payload& payload);
#endif

}  // namespace allocation_recorder::testing

#endif  // COMPONENTS_ALLOCATION_RECORDER_TESTING_CRASH_VERIFICATION_H_
