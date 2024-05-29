// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permissions_android_feature_map.h"

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/permissions/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/permissions/android/core_jni/PermissionsAndroidFeatureMap_jni.h"

namespace permissions {

namespace {

// Array of features exposed through the Java PermissionsAndroidFeatureMap API.
// Entries in this array may either refer to features defined in the header of
// this file or in other locations in the code base (e.g.
// components/permissions/features.h).
const base::Feature* kFeaturesExposedToJava[] = {
    &kAndroidApproximateLocationPermissionSupport,
    &kAndroidCancelPermissionPromptOnTouchOutside,
    &features::kOneTimePermission,
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

// Enables tapping outside the permission prompt scrim to dismiss a permission
// prompt. Do not remove flag (killswitch).
BASE_FEATURE(kAndroidCancelPermissionPromptOnTouchOutside,
             "AndroidCancelPermissionPromptOnTouchOutside",
             base::FEATURE_ENABLED_BY_DEFAULT);

static jlong JNI_PermissionsAndroidFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace permissions
