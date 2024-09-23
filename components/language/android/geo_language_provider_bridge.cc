// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_array.h"
#include "components/language/content/browser/geo_language_provider.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/language/android/jni_headers/GeoLanguageProviderBridge_jni.h"

using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

static ScopedJavaLocalRef<jobjectArray>
JNI_GeoLanguageProviderBridge_GetCurrentGeoLanguages(JNIEnv* env) {
  const std::vector<std::string> current_geo_languages =
      language::GeoLanguageProvider::GetInstance()->CurrentGeoLanguages();
  return ToJavaArrayOfStrings(env, current_geo_languages);
}
