// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_INPUT_ANDROID_INPUT_CALLBACK_H_
#define COMPONENTS_VIZ_SERVICE_INPUT_ANDROID_INPUT_CALLBACK_H_

#include "base/memory/raw_ptr.h"
#include "components/viz/common/surfaces/frame_sink_id.h"

struct AInputEvent;

namespace viz {

class AndroidInputCallbackClient {
 public:
  virtual bool OnMotionEvent(AInputEvent* input_event,
                             const FrameSinkId& root_frame_sink_id) = 0;
};

class AndroidInputCallback {
 public:
  AndroidInputCallback(const FrameSinkId& root_frame_sink_id,
                       AndroidInputCallbackClient* client);

  static bool OnMotionEventThunk(void* context, AInputEvent* input_event);

  bool OnMotionEvent(AInputEvent* input_event);

 private:
  FrameSinkId root_frame_sink_id_;
  raw_ptr<AndroidInputCallbackClient> client_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_INPUT_ANDROID_INPUT_CALLBACK_H_
