// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_capture/common/content_capture_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/content_capture/android/jni_headers/ContentCaptureFeatures_jni.h"

static jboolean JNI_ContentCaptureFeatures_IsEnabled(JNIEnv* env) {
  return content_capture::features::IsContentCaptureEnabled();
}

static jboolean
JNI_ContentCaptureFeatures_ShouldTriggerContentCaptureForExperiment(
    JNIEnv* env) {
  return content_capture::features::ShouldTriggerContentCaptureForExperiment();
}
