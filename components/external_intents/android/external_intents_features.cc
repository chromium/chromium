// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/external_intents/android/external_intents_features.h"

#include <jni.h>
#include <stddef.h>

#include <string>

#include "base/android/jni_string.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/external_intents/android/jni_headers/ExternalIntentsFeatures_jni.h"

namespace external_intents {

namespace {

// Array of features exposed through the Java ExternalIntentsFeatures API.
const base::Feature* const kFeaturesExposedToJava[] = {
    &kExternalNavigationDebugLogs, &kNavigationCaptureRefactorAndroid};
}  // namespace

// Alphabetical:

BASE_FEATURE(kExternalNavigationDebugLogs, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNavigationCaptureRefactorAndroid,
             base::FEATURE_DISABLED_BY_DEFAULT);

static int64_t JNI_ExternalIntentsFeatures_GetFeature(JNIEnv* env,
                                                      int32_t ordinal) {
  return reinterpret_cast<int64_t>(
      UNSAFE_TODO(kFeaturesExposedToJava[ordinal]));
}

}  // namespace external_intents

DEFINE_JNI(ExternalIntentsFeatures)
