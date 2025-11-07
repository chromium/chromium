// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/android/android_input_callback.h"

#include <android/input.h>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace input {

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(AInputEventType)
enum class AInputEventType {
  kUnknown = 0,
  kKey = 1,
  kMotion = 2,
  kFocus = 3,
  kCapture = 4,
  kDrag = 5,
  kTouchMode = 6,
  kMaxValue = kTouchMode,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AInputEventType)

AInputEventType ToAInputEventType(const AInputEvent* key_event) {
  const int key_event_type = AInputEvent_getType(key_event);
  if (key_event_type < static_cast<int>(AInputEventType::kKey) ||
      key_event_type > static_cast<int>(AInputEventType::kMaxValue)) {
    return AInputEventType::kUnknown;
  }
  return static_cast<AInputEventType>(key_event_type);
}

}  // namespace

AndroidInputCallback::AndroidInputCallback(
    const viz::FrameSinkId& root_frame_sink_id,
    AndroidInputCallbackClient* client)
    : root_frame_sink_id_(root_frame_sink_id), client_(client) {
  CHECK(client_ != nullptr);
}

AndroidInputCallback::~AndroidInputCallback() {
  for (auto& observer : observers_) {
    observer.OnCallbackDestroyed();
  }
}

// static
bool AndroidInputCallback::OnMotionEventThunk(void* context,
                                              AInputEvent* input_event) {
  CHECK(context != nullptr);
  AndroidInputCallback* listener =
      reinterpret_cast<AndroidInputCallback*>(context);
  return listener->OnMotionEvent(base::android::ScopedInputEvent(input_event));
}

bool AndroidInputCallback::OnKeyEventThunk(void* context,
                                           AInputEvent* key_event) {
  base::android::ScopedInputEvent scoped_key_event(key_event);
  // In https://crbug.com/441364240 we found out Viz is receiving some key
  // events due to race in getting focus at chrome startup. Not having a key
  // event callback makes the app ANR.
  TRACE_EVENT("input", "AndroidInputCallback::OnKeyEventThunk");
  UMA_HISTOGRAM_ENUMERATION("Android.InputOnViz.Viz.DroppedKeyEvent.Type",
                            ToAInputEventType(key_event));
  // We don't expect to receive key events on Viz.
  return false;
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
