// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_INPUT_MANAGER_OPERATION_TRACKER_H_
#define COMPONENTS_INPUT_INPUT_MANAGER_OPERATION_TRACKER_H_

#include <optional>

#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "components/viz/common/surfaces/frame_sink_id.h"

namespace input {

class InputManagerOperationTracker {
 public:
  struct Operation {
    enum class Type {
      kSetupRenderInputRouter = 0,
      kOnCreateCompositorFrameSink = 1,
      kOnDestroyedCompositorFrameSink = 2,
      kOnRegisteredFrameSinkHierarchy = 3,
      kOnUnregisteredFrameSinkHierarchy = 4,
      kStateOnTouchTransfer = 5,
      kCreateOrReuseAndroidInputReceiver = 6,
      kOnMotionEvent = 7,
    } type;
    base::TimeTicks start_time;
    base::TimeDelta duration;
    std::optional<viz::FrameSinkId> frame_sink_id = std::nullopt;
  };
  virtual void AddOperation(const Operation& operation) = 0;
};

class COMPONENT_EXPORT(INPUT) InputManagerScopedOperation {
 public:
  InputManagerScopedOperation(
      InputManagerOperationTracker& operation_tracker,
      InputManagerOperationTracker::Operation::Type type,
      std::optional<viz::FrameSinkId> frame_sink_id = std::nullopt);

  ~InputManagerScopedOperation();

 private:
  InputManagerOperationTracker::Operation operation_;
  const raw_ref<InputManagerOperationTracker> operation_tracker_;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_INPUT_MANAGER_OPERATION_TRACKER_H_
