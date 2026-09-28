// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/feature_map.h"
#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/policy/core/common/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/policy/android/jni_headers/PolicyFeatureMap_jni.h"

namespace policy {

namespace {
// Array of features exposed through the Java PolicyFeatures API.
const base::Feature* const kFeaturesExposedToJava[] = {
    &features::kAndroidUseAdminsForEnterpriseInfo,
    &features::kUseRestrictionsManagerInAppRestrictionsProvider,
};

base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static int64_t JNI_PolicyFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFeatureMap());
}
}  // namespace policy

DEFINE_JNI(PolicyFeatureMap)
