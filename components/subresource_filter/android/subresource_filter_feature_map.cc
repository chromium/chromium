// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/subresource_filter/android/subresource_filter_jni_headers/SubresourceFilterFeatureMap_jni.h"

namespace subresource_filter {

namespace {

// Array of features exposed through the Java SubresourceFilterFeatureMap API.
// Entries in this array may refer to features either defined in
// components/subresource_filter/core/browser/subresource_filter_features.h or
// in other locations in the code base (e.g. content_features.h).
const base::Feature* const kFeaturesExposedToJava[] = {
    &kSafeBrowsingSubresourceFilter,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_SubresourceFilterFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace subresource_filter
