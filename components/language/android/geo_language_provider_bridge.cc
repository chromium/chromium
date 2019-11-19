// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "components/language/android/jni_headers/GeoLanguageProviderBridge_jni.h"
#include "components/language/content/browser/geo_language_provider.h"

static void JNI_GeoLanguageProviderBridge_GetCurrentGeoLanguages(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& set) {
  const std::vector<std::string> current_geo_languages =
      language::GeoLanguageProvider::GetInstance()->CurrentGeoLanguages();
  for (const auto& language : current_geo_languages) {
    Java_GeoLanguageProviderBridge_addGeoLanguageToSet(
        env, set, base::android::ConvertUTF8ToJavaString(env, language));
  }
}
