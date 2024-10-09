// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/android/scoped_input_receiver.h"

#include "base/android/android_input_receiver_compat.h"
#include "base/check.h"

namespace input {

ScopedInputReceiver::ScopedInputReceiver(ALooper* looper,
                                         AInputTransferToken* input_token,
                                         ASurfaceControl* surface_control,
                                         AInputReceiverCallbacks* callbacks) {
  CHECK(base::AndroidInputReceiverCompat::IsSupportAvailable());
  a_input_receiver_ = base::AndroidInputReceiverCompat::GetInstance()
                          .AInputReceiver_createUnbatchedInputReceiverFn(
                              looper, input_token, surface_control, callbacks);
}

ScopedInputReceiver::~ScopedInputReceiver() {
  DestroyIfNeeded();
}

ScopedInputReceiver::ScopedInputReceiver(ScopedInputReceiver&& other)
    : a_input_receiver_(other.a_input_receiver_) {
  other.a_input_receiver_ = nullptr;
}

ScopedInputReceiver& ScopedInputReceiver::operator=(
    ScopedInputReceiver&& other) {
  if (this != &other) {
    DestroyIfNeeded();
    a_input_receiver_ = other.a_input_receiver_;
    other.a_input_receiver_ = nullptr;
  }
  return *this;
}

void ScopedInputReceiver::DestroyIfNeeded() {
  if (a_input_receiver_ == nullptr) {
    return;
  }
  // Not calling release due to AOSP crash on calling the API -
  // b/368251173.
  // base::AndroidInputReceiverCompat::GetInstance()
  //   .AInputReceiver_releaseFn(a_input_receiver_);
  a_input_receiver_ = nullptr;
}

}  // namespace input
