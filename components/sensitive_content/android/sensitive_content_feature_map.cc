// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/sensitive_content/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/sensitive_content/jni_headers/SensitiveContentFeatureMap_jni.h"

namespace sensitive_content {
namespace {

// Array of sensitive content features exposed through the Java
// SensitiveContentFeatureMap API. Entries in this array refer to features
// defined in components/sensitive_content/features.h.
const base::Feature* const kFeaturesExposedToJava[] = {
    &features::kSensitiveContentWhileSwitchingTabs,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      base::ToVector(kFeaturesExposedToJava));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_SensitiveContentFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace sensitive_content
