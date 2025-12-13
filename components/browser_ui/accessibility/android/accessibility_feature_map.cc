// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/browser_ui/accessibility/android/accessibility_feature_map_jni_headers/AccessibilityFeatureMap_jni.h"
#include "components/browser_ui/accessibility/android/features.h"

namespace browser_ui {

namespace {

// Array of features exposed through the Java AccessibilityFeatureMap API.
// Entries in this array may either refer to features defined in
// components/browser_ui/accessibility/android/features.h or in other
// locations in the code base (e.g. content_features.h).
const base::Feature* const kFeaturesExposedToJava[] = {
    &kAndroidZoomIndicator,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_AccessibilityFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace browser_ui

DEFINE_JNI(AccessibilityFeatureMap)
