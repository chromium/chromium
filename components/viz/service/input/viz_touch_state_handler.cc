// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/viz_touch_state_handler.h"

#include <utility>

#include "base/logging.h"
#include "components/viz/common/input/viz_touch_state.h"

namespace viz {

VizTouchStateHandler::VizTouchStateHandler() {
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(sizeof(VizTouchState));

  if (mapped_region.IsValid()) {
    viz_touch_state_writable_mapping_ = std::move(mapped_region.mapping);
    viz_touch_state_read_only_region_ = std::move(mapped_region.region);
    viz_touch_state_ =
        viz_touch_state_writable_mapping_.GetMemoryAs<VizTouchState>();
    new (viz_touch_state_) VizTouchState();
  } else {
    LOG(ERROR) << "Failed to create VizTouchState shared memory.";
  }
}

VizTouchStateHandler::~VizTouchStateHandler() = default;

void VizTouchStateHandler::OnMotionEvent(
    const base::android::ScopedInputEvent& input_event) {
  if (!viz_touch_state_) {
    return;
  }

  const int action = AMotionEvent_getAction(input_event.a_input_event()) &
                     AMOTION_EVENT_ACTION_MASK;

  if (action == AMOTION_EVENT_ACTION_DOWN) {
    viz_touch_state_->is_sequence_active.store(true, std::memory_order_release);
  } else if (action == AMOTION_EVENT_ACTION_UP ||
             action == AMOTION_EVENT_ACTION_CANCEL) {
    viz_touch_state_->is_sequence_active.store(false,
                                               std::memory_order_release);
  }
}

void VizTouchStateHandler::OnCallbackDestroyed() {
  if (viz_touch_state_) {
    viz_touch_state_->is_sequence_active.store(false,
                                               std::memory_order_release);
  }
}

base::ReadOnlySharedMemoryRegion
VizTouchStateHandler::DuplicateVizTouchStateRegion() const {
  if (viz_touch_state_read_only_region_.IsValid()) {
    return viz_touch_state_read_only_region_.Duplicate();
  }
  return base::ReadOnlySharedMemoryRegion();
}

void VizTouchStateHandler::UpdateLastTransferredBackDownTimeMs(
    int64_t down_time_ms) {
  if (viz_touch_state_) {
    viz_touch_state_->last_transferred_back_down_time_ms.store(
        down_time_ms, std::memory_order_release);
  }
}

}  // namespace viz
