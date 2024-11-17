// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_ANDROID_SCOPED_INPUT_RECEIVER_H_
#define COMPONENTS_INPUT_ANDROID_SCOPED_INPUT_RECEIVER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"

struct AInputReceiver;
struct ALooper;
struct AInputTransferToken;
struct ASurfaceControl;
struct AInputReceiverCallbacks;

namespace input {

// Class to manage lifecycle of AInputReceiver. AInputReceiver was
// added in Android V+, this class is expected to be instantiated only when
// running on Android V+. AInputReceiver allows receiving input through
// registered callbacks. NDK documentation for these APIs is not yet live but we
// can find the relevant methods and comments here:
// https://cs.android.com/android/platform/superproject/main/+/main:frameworks/native/include/android/surface_control_input_receiver.h

class COMPONENT_EXPORT(INPUT) ScopedInputReceiver {
 public:
  ScopedInputReceiver(ALooper* looper,
                      AInputTransferToken* input_token,
                      ASurfaceControl* surface_control,
                      AInputReceiverCallbacks* callbacks);
  ~ScopedInputReceiver();

  ScopedInputReceiver(ScopedInputReceiver&& other);
  ScopedInputReceiver& operator=(ScopedInputReceiver&& other);

  // Move only type.
  ScopedInputReceiver(const ScopedInputReceiver&) = delete;
  ScopedInputReceiver& operator=(const ScopedInputReceiver&) = delete;

  explicit operator bool() const { return !!a_input_receiver_; }

  AInputReceiver* a_input_receiver() const { return a_input_receiver_; }

 private:
  void DestroyIfNeeded();

  // RAW_PTR_EXCLUSION: #global-scope
  RAW_PTR_EXCLUSION AInputReceiver* a_input_receiver_ = nullptr;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_ANDROID_SCOPED_INPUT_RECEIVER_H_
