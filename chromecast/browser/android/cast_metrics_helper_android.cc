// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/metrics/cast_metrics_helper.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chromecast/browser/android/jni_headers/CastMetricsHelper_jni.h"

namespace chromecast {
namespace shell {

void JNI_CastMetricsHelper_LogMediaPlay(JNIEnv* env) {
  metrics::CastMetricsHelper::GetInstance()->LogMediaPlay();
}

void JNI_CastMetricsHelper_LogMediaPause(JNIEnv* env) {
  metrics::CastMetricsHelper::GetInstance()->LogMediaPause();
}

}  // namespace shell
}  // namespace chromecast
