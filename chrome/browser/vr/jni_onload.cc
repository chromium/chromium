// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_generator/jni_generator_helper.h"

// This method is required by the module loading backend. And it is supposed to
// register VR's native JNI methods. However, since VR's Android-specific native
// code still lives in the base module, VR's JNI registration is invoked
// manually. Therefore, this function does nothing.
JNI_GENERATOR_EXPORT bool JNI_OnLoad_vr(JNIEnv* env) {
  return true;
}
