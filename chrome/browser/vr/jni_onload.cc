// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/jni_zero.h"

// This method is required by the module loading backend. And it is supposed to
// register VR's native JNI methods. However, since VR's Android-specific native
// code still lives in the base module, VR's JNI registration is invoked
// manually. Therefore, this function does nothing.
JNI_BOUNDARY_EXPORT bool JNI_OnLoad_vr(JNIEnv* env) {
  return true;
}
