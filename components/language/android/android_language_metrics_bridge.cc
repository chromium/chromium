// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "components/language/android/jni_headers/AndroidLanguageMetricsBridge_jni.h"

// Records the HashMetric of |value| in the sparse histogram |histogramName|.
static void JNI_AndroidLanguageMetricsBridge_ReportHashMetricName(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& histogramName,
    const base::android::JavaParamRef<jstring>& value) {
  base::UmaHistogramSparse(
      base::android::ConvertJavaStringToUTF8(histogramName),
      base::HashMetricName(base::android::ConvertJavaStringToUTF8(value)));
}
