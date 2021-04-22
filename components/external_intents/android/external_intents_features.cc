// Copyright 2020 The Chromium Authors. All rights reserved.
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
    &kIntentBlockExternalFormRedirectsNoGesture,
};

}  // namespace

// Alphabetical:
const base::Feature kIntentBlockExternalFormRedirectsNoGesture{
    "IntentBlockExternalFormRedirectsNoGesture",
    base::FEATURE_DISABLED_BY_DEFAULT};

static jlong JNI_ExternalIntentsFeatures_GetFeature(JNIEnv* env, jint ordinal) {
  return reinterpret_cast<jlong>(kFeaturesExposedToJava[ordinal]);
}

}  // namespace external_intents
