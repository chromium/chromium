// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_ANDROID_INPUT_TOKEN_FORWARDER_H_
#define COMPONENTS_INPUT_ANDROID_INPUT_TOKEN_FORWARDER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/component_export.h"

namespace input {

// Allows forwarding InputTransferToken for Viz's InputReceiver back to Browser
// so that it can be used to transfer touch sequence to Viz.
class COMPONENT_EXPORT(INPUT) InputTokenForwarder {
 public:
  static InputTokenForwarder* GetInstance();
  static void SetInstance(InputTokenForwarder* instance);

  virtual void ForwardVizInputTransferToken(
      int surface_id,
      base::android::ScopedJavaGlobalRef<jobject> viz_input_token) = 0;

 protected:
  virtual ~InputTokenForwarder() = default;
};

}  // namespace input

#endif  // COMPONENTS_INPUT_ANDROID_INPUT_TOKEN_FORWARDER_H_
