// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/external_intents/android/external_intents_features.h"

#include <jni.h>
#include <stddef.h>
#include <string>

#include "base/android/jni_string.h"
#include "base/notreached.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/external_intents/android/jni_headers/ExternalIntentsFeatures_jni.h"

namespace external_intents {

namespace {

// Array of features exposed through the Java ExternalIntentsFeatures API.
const base::Feature* kFeaturesExposedToJava[] = {
    &kExternalNavigationDebugLogs, &kBlockFrameRenavigations,
    &kBlockIntentsToSelf, &kTrustedClientGestureBypass};

}  // namespace

// Alphabetical:

BASE_FEATURE(kExternalNavigationDebugLogs,
             "ExternalNavigationDebugLogs",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBlockFrameRenavigations,
             "BlockFrameRenavigations3",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBlockIntentsToSelf,
             "BlockIntentsToSelf",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kTrustedClientGestureBypass,
             "TrustedClientGestureBypass",
             base::FEATURE_ENABLED_BY_DEFAULT);

static jlong JNI_ExternalIntentsFeatures_GetFeature(JNIEnv* env, jint ordinal) {
  return reinterpret_cast<jlong>(kFeaturesExposedToJava[ordinal]);
}

}  // namespace external_intents
