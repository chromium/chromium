// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permissions_android_feature_map.h"

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/permissions/android/core_jni/PermissionsAndroidFeatureMap_jni.h"
#include "components/permissions/features.h"
#include "content/public/common/content_features.h"

namespace permissions {

namespace {

// Array of features exposed through the Java PermissionsAndroidFeatureMap API.
// Entries in this array may either refer to features defined in the header of
// this file or in other locations in the code base (e.g.
// components/permissions/features.h).
const base::Feature* kFeaturesExposedToJava[] = {
    &kAndroidApproximateLocationPermissionSupport,
    &::features::kBlockMidiByDefault,
    &features::kPermissionStorageAccessAPI,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

BASE_FEATURE(kAndroidApproximateLocationPermissionSupport,
             "AndroidApproximateLocationPermissionSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

static jlong JNI_PermissionsAndroidFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace permissions
