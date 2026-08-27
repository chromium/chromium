// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/browser_ui/bottomsheet/android/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/bottomsheet/android/bottomsheet_jni_headers/BottomSheetFeatureMap_jni.h"

namespace browser_ui {

namespace {

// Array of features exposed through the Java BottomSheetFeatureMap API.
// Entries in this array may either refer to features defined in
// components/browser_ui/bottomsheet/android/features.h or in other
// locations in the code base.
const base::Feature* const kFeaturesExposedToJava[] = {
    &kBottomSheetTypes,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static int64_t JNI_BottomSheetFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFeatureMap());
}

}  // namespace browser_ui

DEFINE_JNI(BottomSheetFeatureMap)
