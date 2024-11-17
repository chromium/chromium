// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/android/scoped_input_receiver_callbacks.h"

#include "base/android/android_input_receiver_compat.h"
#include "base/check.h"

namespace input {

ScopedInputReceiverCallbacks::ScopedInputReceiverCallbacks(void* context) {
  CHECK(base::AndroidInputReceiverCompat::IsSupportAvailable());
  a_input_receiver_callbacks_ = base::AndroidInputReceiverCompat::GetInstance()
                                    .AInputReceiverCallbacks_createFn(context);
}

ScopedInputReceiverCallbacks::~ScopedInputReceiverCallbacks() {
  DestroyIfNeeded();
}

ScopedInputReceiverCallbacks::ScopedInputReceiverCallbacks(
    ScopedInputReceiverCallbacks&& other)
    : a_input_receiver_callbacks_(other.a_input_receiver_callbacks_) {
  other.a_input_receiver_callbacks_ = nullptr;
}

ScopedInputReceiverCallbacks& ScopedInputReceiverCallbacks::operator=(
    ScopedInputReceiverCallbacks&& other) {
  if (this != &other) {
    DestroyIfNeeded();
    a_input_receiver_callbacks_ = other.a_input_receiver_callbacks_;
    other.a_input_receiver_callbacks_ = nullptr;
  }
  return *this;
}

void ScopedInputReceiverCallbacks::DestroyIfNeeded() {
  if (a_input_receiver_callbacks_ == nullptr) {
    return;
  }
  base::AndroidInputReceiverCompat::GetInstance()
      .AInputReceiverCallbacks_releaseFn(a_input_receiver_callbacks_);
  a_input_receiver_callbacks_ = nullptr;
}

}  // namespace input
