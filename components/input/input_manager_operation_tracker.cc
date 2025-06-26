// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/input_manager_operation_tracker.h"

namespace input {

InputManagerScopedOperation::InputManagerScopedOperation(
    InputManagerOperationTracker& operation_tracker,
    InputManagerOperationTracker::Operation::Type type,
    std::optional<viz::FrameSinkId> frame_sink_id)
    : operation_tracker_(operation_tracker) {
  operation_.start_time = base::TimeTicks::Now();
  operation_.type = type;
  operation_.frame_sink_id = frame_sink_id;
}

InputManagerScopedOperation::~InputManagerScopedOperation() {
  operation_.duration = base::TimeTicks::Now() - operation_.start_time;
  operation_tracker_->AddOperation(operation_);
}

}  // namespace input
