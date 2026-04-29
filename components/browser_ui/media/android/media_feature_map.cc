// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "media/base/media_switches.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/media/android/media_jni_headers/MediaFeatureMap_jni.h"

namespace browser_ui {

namespace {

// Array of features exposed through the Java MediaFeatureMap API.
const base::Feature* const kFeaturesExposedToJava[] = {
    &media::kPauseMediaOnSystemSleepAndroid,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static int64_t JNI_MediaFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFeatureMap());
}

}  // namespace browser_ui

DEFINE_JNI(MediaFeatureMap)
