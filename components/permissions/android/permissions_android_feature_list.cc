// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/permissions_android_feature_list.h"
#include "base/android/jni_string.h"
#include "base/notreached.h"
#include "components/permissions/android/jni_headers/PermissionsAndroidFeatureList_jni.h"
#include "components/permissions/features.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace permissions {

namespace {

// Array of features exposed through the Java ContentFeatureList API. Entries in
// this array may either refer to features defined in the header of this file or
// in other locations in the code base (e.g. content_features.h).
const base::Feature* kFeaturesExposedToJava[] = {
    &kAndroidApproximateLocationPermissionSupport,
    &permissions::features::kBlockMidiByDefault,
};

const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (const base::Feature* feature : kFeaturesExposedToJava) {
    if (feature->name == feature_name)
      return feature;
  }
  NOTREACHED() << "Queried feature not found in PermissionsAndroidFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

BASE_FEATURE(kAndroidApproximateLocationPermissionSupport,
             "AndroidApproximateLocationPermissionSupport",
             base::FEATURE_ENABLED_BY_DEFAULT);

static jboolean JNI_PermissionsAndroidFeatureList_IsInitialized(JNIEnv* env) {
  return !!base::FeatureList::GetInstance();
}

static jboolean JNI_PermissionsAndroidFeatureList_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

}  // namespace permissions
