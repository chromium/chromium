// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/android/scoped_input_receiver.h"

#include "base/android/android_info.h"
#include "base/android/android_input_receiver_compat.h"

namespace input {

ScopedInputReceiver::ScopedInputReceiver(AInputReceiver* a_input_receiver)
    : a_input_receiver_(a_input_receiver) {}

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
  if (base::android::android_info::sdk_int() >=
      base::android::android_info::SdkVersion::SDK_VERSION_BAKLAVA) {
    // Calling AInputReceiver_release on Android V, results in app crash.
    base::AndroidInputReceiverCompat::GetInstance().AInputReceiver_releaseFn(
        a_input_receiver_);
  }
  a_input_receiver_ = nullptr;
}

}  // namespace input
