// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_ANDROID_SCOPED_INPUT_RECEIVER_CALLBACKS_H_
#define COMPONENTS_INPUT_ANDROID_SCOPED_INPUT_RECEIVER_CALLBACKS_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"

struct AInputReceiverCallbacks;

namespace input {

// Class to manage lifecycle of AInputReceiverCallbacks. AInputReceiverCallbacks
// was added in Android V+ and the class is expected to be instantiated only
// when running on Android V+.

class COMPONENT_EXPORT(INPUT) ScopedInputReceiverCallbacks {
 public:
  explicit ScopedInputReceiverCallbacks(void* context);
  ~ScopedInputReceiverCallbacks();

  ScopedInputReceiverCallbacks(ScopedInputReceiverCallbacks&& other);
  ScopedInputReceiverCallbacks& operator=(ScopedInputReceiverCallbacks&& other);

  // Move only type.
  ScopedInputReceiverCallbacks(const ScopedInputReceiverCallbacks&) = delete;
  ScopedInputReceiverCallbacks& operator=(const ScopedInputReceiverCallbacks&) =
      delete;

  explicit operator bool() const { return !!a_input_receiver_callbacks_; }

  AInputReceiverCallbacks* a_input_receiver_callbacks() const {
    return a_input_receiver_callbacks_;
  }

 private:
  void DestroyIfNeeded();

  // RAW_PTR_EXCLUSION: #global-scope
  RAW_PTR_EXCLUSION AInputReceiverCallbacks* a_input_receiver_callbacks_ =
      nullptr;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_ANDROID_SCOPED_INPUT_RECEIVER_CALLBACKS_H_
