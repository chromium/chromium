// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/tab_groups/features.h"
#include "components/tab_groups/tab_groups_jni_headers/TabGroupsFeatureMap_jni.h"

namespace tab_groups_android {

namespace {

// Array of features exposed through the Java TabGroupsFeatureMap API.
// Entries in this array may either refer to features defined in
// components/tab_groups/features.h or in other locations in the code
// base.
const base::Feature* const kFeaturesExposedToJava[] = {
    &tab_groups::kUpdateTabGroupColors,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static int64_t JNI_TabGroupsFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFeatureMap());
}

DEFINE_JNI(TabGroupsFeatureMap)

}  // namespace tab_groups_android
