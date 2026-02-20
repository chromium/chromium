// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/util/features.h"

#include <jni.h>
#include <stddef.h>

#include <string>

#include "base/android/jni_string.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/notreached.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/util_jni/EmbedderSupportFeatures_jni.h"

namespace embedder_support::features {

namespace {

// Array of features exposed through the Java EmbedderSupportFeatures API.
const base::Feature* const kFeaturesExposedToJava[] = {
    &kAndroidChromeSchemeNavigationKillSwitch};
}  // namespace

BASE_FEATURE(kAndroidChromeSchemeNavigationKillSwitch,
             base::FEATURE_ENABLED_BY_DEFAULT);

static int64_t JNI_EmbedderSupportFeatures_GetFeature(JNIEnv* env,
                                                      int32_t ordinal) {
  return reinterpret_cast<int64_t>(
      UNSAFE_TODO(kFeaturesExposedToJava[ordinal]));
}

BASE_FEATURE(kInputStreamOptimizations, base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace embedder_support::features

DEFINE_JNI(EmbedderSupportFeatures)
