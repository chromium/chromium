// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_VIZ_TOUCH_STATE_HANDLER_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_VIZ_TOUCH_STATE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "components/input/android/android_input_callback.h"
#include "components/viz/common/input/viz_touch_state.h"
#include "components/viz/service/viz_service_export.h"
#include "ui/events/android/motion_event_android.h"

namespace viz {

class VIZ_SERVICE_EXPORT VizTouchStateHandler
    : public input::AndroidInputCallback::Observer {
 public:
  VizTouchStateHandler();
  ~VizTouchStateHandler() override;

  // input::AndroidInputCallback::Observer:
  void OnMotionEvent(
      const base::android::ScopedInputEvent& input_event) override;
  void OnCallbackDestroyed() override;

  base::ReadOnlySharedMemoryRegion DuplicateVizTouchStateRegion() const;

  virtual void UpdateLastTransferredBackDownTimeMs(int64_t down_time_ms);

 private:
  // Writable shared memory for VizTouchState. This is created and owned by Viz.
  base::WritableSharedMemoryMapping viz_touch_state_writable_mapping_;
  // Read-only shared memory region for VizTouchState to be sent to the browser.
  base::ReadOnlySharedMemoryRegion viz_touch_state_read_only_region_;
  raw_ptr<VizTouchState> viz_touch_state_ = nullptr;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_VIZ_TOUCH_STATE_HANDLER_H_
