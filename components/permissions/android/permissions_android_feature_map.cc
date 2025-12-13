// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permissions_android_feature_map.h"

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/content_settings/core/common/features.h"
#include "components/permissions/features.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/features_generated.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/permissions/android/core_jni/PermissionsAndroidFeatureMap_jni.h"

namespace permissions {

namespace {

// Array of features exposed through the Java PermissionsAndroidFeatureMap API.
// Entries in this array may either refer to features defined in the header of
// this file or in other locations in the code base (e.g.
// components/permissions/features.h).
const base::Feature* const kFeaturesExposedToJava[] = {
    &kAndroidCancelPermissionPromptOnTouchOutside,
    &kPermissionsAndroidClapperLoud,
    &kPermissionsAndroidClapperQuiet,
    &features::kPermissionHeuristicAutoGrant,
    &content_settings::features::kApproximateGeolocationPermission,
    &media::kAutoPictureInPictureAndroid,
    &blink::features::kPermissionElement,
    &blink::features::kBypassPepcSecurityForTesting,
    &blink::features::kGeolocationElement,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

// Enables tapping outside the permission prompt scrim to dismiss a permission
// prompt. Do not remove flag (killswitch).
BASE_FEATURE(kAndroidCancelPermissionPromptOnTouchOutside,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables the loud version of the Clapper permission prompt.
BASE_FEATURE(kPermissionsAndroidClapperLoud, base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the quiet version of the Clapper permission prompt.
BASE_FEATURE(kPermissionsAndroidClapperQuiet,
             base::FEATURE_DISABLED_BY_DEFAULT);

static jlong JNI_PermissionsAndroidFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace permissions

DEFINE_JNI(PermissionsAndroidFeatureMap)
