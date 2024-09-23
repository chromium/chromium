// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/feature_map.h"
#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/no_destructor.h"
#include "components/sync/base/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/sync/android/jni_headers/SyncFeatureMap_jni.h"

namespace syncer {

namespace {

// Array of features exposed through the Java SyncFeatureMap.
const base::Feature* const kFeaturesExposedToJava[] = {
    &kSyncEnableBookmarksInTransportMode};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_SyncFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace syncer
