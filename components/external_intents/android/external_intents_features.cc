// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/external_intents/android/external_intents_features.h"

#include <jni.h>
#include <stddef.h>
#include <string>

#include "base/android/jni_string.h"
#include "base/notreached.h"
#include "components/external_intents/android/jni_headers/ExternalIntentsFeatures_jni.h"

namespace external_intents {

namespace {

// Array of features exposed through the Java ExternalIntentsFeatures API.
const base::Feature* kFeaturesExposedToJava[] = {
    &kExternalNavigationDebugLogs,       &kExternalNavigationSubframeRedirects,
    &kBlockSubframeIntentToSelf,         &kBlockFrameRenavigations,
    &kDoNotRequireSpecializedCCTHandler, &kBlockIntentsToSelf};

}  // namespace

// Alphabetical:

BASE_FEATURE(kExternalNavigationDebugLogs,
             "ExternalNavigationDebugLogs",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExternalNavigationSubframeRedirects,
             "ExternalNavigationSubframeRedirects",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBlockSubframeIntentToSelf,
             "BlockSubframeIntentToSelf",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBlockFrameRenavigations,
             "BlockFrameRenavigations",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDoNotRequireSpecializedCCTHandler,
             "DoNotRequireSpecializedCCTHandler",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kBlockIntentsToSelf,
             "BlockIntentsToSelf",
             base::FEATURE_ENABLED_BY_DEFAULT);

static jlong JNI_ExternalIntentsFeatures_GetFeature(JNIEnv* env, jint ordinal) {
  return reinterpret_cast<jlong>(kFeaturesExposedToJava[ordinal]);
}

}  // namespace external_intents
