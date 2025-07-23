// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/android/android_input_callback.h"

#include "base/check.h"

namespace input {

AndroidInputCallback::AndroidInputCallback(
    const viz::FrameSinkId& root_frame_sink_id,
    AndroidInputCallbackClient* client)
    : root_frame_sink_id_(root_frame_sink_id), client_(client) {
  CHECK(client_ != nullptr);
}

AndroidInputCallback::~AndroidInputCallback() = default;

// static
bool AndroidInputCallback::OnMotionEventThunk(void* context,
                                              AInputEvent* input_event) {
  CHECK(context != nullptr);
  AndroidInputCallback* listener =
      reinterpret_cast<AndroidInputCallback*>(context);
  return listener->OnMotionEvent(base::android::ScopedInputEvent(input_event));
}

bool AndroidInputCallback::OnMotionEvent(
    base::android::ScopedInputEvent input_event) {
  for (auto& observer : observers_) {
    observer.OnMotionEvent(input_event);
  }
  return client_->OnMotionEvent(std::move(input_event), root_frame_sink_id_);
}

void AndroidInputCallback::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void AndroidInputCallback::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

}  // namespace input
