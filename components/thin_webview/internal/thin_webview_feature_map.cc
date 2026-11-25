// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/thin_webview/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/thin_webview/internal/jni_headers/ThinWebViewFeatureMap_jni.h"

namespace thin_webview::android {

namespace {

// Array of features exposed through the Java ThinWebViewFeatureMap API.
const base::Feature* const kFeaturesExposedToJava[] = {
    &kThinWebViewReleaseSurfaceTextureOnDestroy,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static int64_t JNI_ThinWebViewFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFeatureMap());
}

}  // namespace thin_webview::android

DEFINE_JNI(ThinWebViewFeatureMap)
