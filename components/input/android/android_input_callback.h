// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_ANDROID_ANDROID_INPUT_CALLBACK_H_
#define COMPONENTS_INPUT_ANDROID_ANDROID_INPUT_CALLBACK_H_

#include "base/android/scoped_input_event.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/viz/common/surfaces/frame_sink_id.h"

struct AInputEvent;

namespace input {

class AndroidInputCallbackClient {
 public:
  virtual bool OnMotionEvent(base::android::ScopedInputEvent input_event,
                             const viz::FrameSinkId& root_frame_sink_id) = 0;
};

class COMPONENT_EXPORT(INPUT) AndroidInputCallback {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnMotionEvent(
        const base::android::ScopedInputEvent& input_event) = 0;
    virtual void OnCallbackDestroyed() = 0;
  };

  AndroidInputCallback(const viz::FrameSinkId& root_frame_sink_id,
                       AndroidInputCallbackClient* client);
  ~AndroidInputCallback();

  static bool OnMotionEventThunk(void* context, AInputEvent* input_event);
  static bool OnKeyEventThunk(void* context, AInputEvent* key_event);

  bool OnMotionEvent(base::android::ScopedInputEvent input_event);

  const viz::FrameSinkId& root_frame_sink_id() const {
    return root_frame_sink_id_;
  }

  void set_root_frame_sink_id(const viz::FrameSinkId& frame_sink_id) {
    root_frame_sink_id_ = frame_sink_id;
  }

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

 private:
  viz::FrameSinkId root_frame_sink_id_;
  raw_ptr<AndroidInputCallbackClient> client_;
  base::ObserverList<Observer> observers_;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_ANDROID_ANDROID_INPUT_CALLBACK_H_
