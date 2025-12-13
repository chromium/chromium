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
    &kProbeStylusWritingInBackground,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_StylusHandwritingFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

// Android only features.

// Cache Stylus related settings
BASE_FEATURE(kCacheStylusSettings, base::FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kProbeStylusWritingInBackground,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace stylus_handwriting::android

DEFINE_JNI(StylusHandwritingFeatureMap)
