// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "components/language/android/jni_headers/AndroidLanguageMetricsBridge_jni.h"

const char kTranslateExplicitLanguageAskLanguageAdded[] =
    "Translate.ExplicitLanguageAsk.LanguageAdded";
const char kTranslateExplicitLanguageAskLanguageRemoved[] =
    "Translate.ExplicitLanguageAsk.LanguageRemoved";

// Called when a user adds or removes a language from the list of languages they
// can read using the Explicit Language Ask prompt at 2nd run.
static void
JNI_AndroidLanguageMetricsBridge_ReportExplicitLanguageAskStateChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& language,
    const jboolean added) {
  base::UmaHistogramSparse(
      added ? kTranslateExplicitLanguageAskLanguageAdded
            : kTranslateExplicitLanguageAskLanguageRemoved,
      base::HashMetricName(base::android::ConvertJavaStringToUTF8(language)));
}
