// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/webapps/browser/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webapps/browser/android/webapps_jni_headers/WebappsFeatureMap_jni.h"

namespace webapps {

namespace {

// Array of features exposed through the Java WebappsFeatureMap API.
const base::Feature* const kFeaturesExposedToJava[] = {
    &webapps::features::kAlwaysShowInstallDisambiguationDialog,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static int64_t JNI_WebappsFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFeatureMap());
}

DEFINE_JNI(WebappsFeatureMap)

}  // namespace webapps
