// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/browser_ui/site_settings/android/features.h"
#include "components/browser_ui/site_settings/android/site_settings_jni_headers/SiteSettingsFeatureMap_jni.h"

namespace browser_ui {

namespace {

// Array of features exposed through the Java SiteSettingsFeatureMap API.
// Entries in this array may either refer to features defined in
// components/browser_ui/site_settings/android/features.h or in other locations
// in the code base (e.g. content_features.h).
const base::Feature* const kFeaturesExposedToJava[] = {
    &kSiteDataImprovements,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_SiteSettingsFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace browser_ui
