// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/input/android_input_callback.h"

#include "base/check.h"

namespace viz {

AndroidInputCallback::AndroidInputCallback(
    const FrameSinkId& root_frame_sink_id,
    AndroidInputCallbackClient* client)
    : root_frame_sink_id_(root_frame_sink_id), client_(client) {
  CHECK(client_ != nullptr);
}

// static
bool AndroidInputCallback::OnMotionEventThunk(void* context,
                                              AInputEvent* input_event) {
  CHECK(context != nullptr);
  AndroidInputCallback* listener =
      reinterpret_cast<AndroidInputCallback*>(context);
  return listener->OnMotionEvent(input_event);
}

bool AndroidInputCallback::OnMotionEvent(AInputEvent* input_event) {
  return client_->OnMotionEvent(input_event, root_frame_sink_id_);
}

}  // namespace viz
