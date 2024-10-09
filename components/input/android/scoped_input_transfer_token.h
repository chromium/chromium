// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_ANDROID_SCOPED_INPUT_TRANSFER_TOKEN_H_
#define COMPONENTS_INPUT_ANDROID_SCOPED_INPUT_TRANSFER_TOKEN_H_

#include <jni.h>

#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"

struct AInputTransferToken;

namespace input {

// Class to manage lifecycle of AInputTransferToken. AInputTransferToken was
// added in Android V+ and the class is expected to be instantiated only when
// running on Android V+.

class COMPONENT_EXPORT(INPUT) ScopedInputTransferToken {
 public:
  explicit ScopedInputTransferToken(const jobject& input_transfer_token);
  ~ScopedInputTransferToken();

  ScopedInputTransferToken(ScopedInputTransferToken&& other);
  ScopedInputTransferToken& operator=(ScopedInputTransferToken&& other);

  // Move only type.
  ScopedInputTransferToken(const ScopedInputTransferToken&) = delete;
  ScopedInputTransferToken& operator=(const ScopedInputTransferToken&) = delete;

  explicit operator bool() const { return !!a_input_transfer_token_; }

  AInputTransferToken* a_input_transfer_token() const {
    return a_input_transfer_token_;
  }

 private:
  explicit ScopedInputTransferToken(AInputTransferToken* a_native_window);

  void DestroyIfNeeded();

  // RAW_PTR_EXCLUSION: #global-scope
  RAW_PTR_EXCLUSION AInputTransferToken* a_input_transfer_token_ = nullptr;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_ANDROID_SCOPED_INPUT_TRANSFER_TOKEN_H_
