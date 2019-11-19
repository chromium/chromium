// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/android/jni_headers/ContentCaptureFeatures_jni.h"
#include "components/content_capture/common/content_capture_features.h"

static jboolean JNI_ContentCaptureFeatures_IsEnabled(JNIEnv* env) {
  return content_capture::features::IsContentCaptureEnabled();
}
