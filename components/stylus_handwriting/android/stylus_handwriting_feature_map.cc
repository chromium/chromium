// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/stylus_handwriting/android/stylus_handwriting_feature_map.h"

#include "base/android/feature_map.h"
#include "base/no_destructor.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/stylus_handwriting/android/feature_list_jni/StylusHandwritingFeatureMap_jni.h"

namespace stylus_handwriting::android {
namespace {

const base::Feature* const kFeaturesExposedToJava[] = {
    &kCacheStylusSettings,
    &kUseHandwritingInitiator,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_StylusHandwritingFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

// Android only features.

// Cache Stylus related settings
BASE_FEATURE(kCacheStylusSettings,
             "CacheStylusSettings",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Initiate handwriting based on the distance between touch events compared to
// the current handwriting touch slop rather than relying on scroll events for
// initiation.
BASE_FEATURE(kUseHandwritingInitiator,
             "UseHandwritingInitiator",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace stylus_handwriting::android
