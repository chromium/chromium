// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/android/scoped_input_transfer_token.h"

#include "base/android/android_input_receiver_compat.h"
#include "base/android/jni_android.h"

namespace input {

ScopedInputTransferToken::ScopedInputTransferToken(
    const jobject& input_transfer_token) {
  CHECK(base::AndroidInputReceiverCompat::IsSupportAvailable());
  JNIEnv* env = base::android::AttachCurrentThread();
  a_input_transfer_token_ =
      base::AndroidInputReceiverCompat::GetInstance()
          .AInputTransferToken_fromJavaFn(env, input_transfer_token);
}

ScopedInputTransferToken::~ScopedInputTransferToken() {
  DestroyIfNeeded();
}

ScopedInputTransferToken::ScopedInputTransferToken(
    ScopedInputTransferToken&& other)
    : a_input_transfer_token_(other.a_input_transfer_token_) {
  other.a_input_transfer_token_ = nullptr;
}

ScopedInputTransferToken& ScopedInputTransferToken::operator=(
    ScopedInputTransferToken&& other) {
  if (this != &other) {
    DestroyIfNeeded();
    a_input_transfer_token_ = other.a_input_transfer_token_;
    other.a_input_transfer_token_ = nullptr;
  }
  return *this;
}

void ScopedInputTransferToken::DestroyIfNeeded() {
  if (a_input_transfer_token_ == nullptr) {
    return;
  }
  base::AndroidInputReceiverCompat::GetInstance().AInputTransferToken_releaseFn(
      a_input_transfer_token_);
  a_input_transfer_token_ = nullptr;
}

}  // namespace input
