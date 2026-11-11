// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_STREAM_TRANSPORT_TEST_SUPPORT_PROTO_STREAM_WAIT_FOR_H_
#define COMPONENTS_STREAM_TRANSPORT_TEST_SUPPORT_PROTO_STREAM_WAIT_FOR_H_

#include "base/functional/function_ref.h"

namespace stream_transport {

// Runs the run loop until `condition` is true, re-evaluating it after
// every main-thread task, and returns whether it became true (a large
// task budget bounds the wait, so a test fails instead of hanging).
//
// base::test::RunUntil() is unsuitable in the transport fixtures: it only
// evaluates its condition when the thread goes fully idle, and under
// MOCK_TIME the clock then auto-advances through the reconnect and stall
// timers, so a usable idle moment may never arrive.
// TODO(crbug.com/376085325): Delete this in favor of base::test::RunUntil()
// once it can evaluate under MOCK_TIME.
[[nodiscard]] bool WaitFor(base::FunctionRef<bool()> condition);

}  // namespace stream_transport

#endif  // COMPONENTS_STREAM_TRANSPORT_TEST_SUPPORT_PROTO_STREAM_WAIT_FOR_H_
