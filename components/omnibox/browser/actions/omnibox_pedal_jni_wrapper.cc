// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include "base/android/jni_string.h"
#include "components/omnibox/browser/jni_headers/HistoryClustersAction_jni.h"
#include "components/omnibox/browser/jni_headers/OmniboxPedal_jni.h"
#include "url/android/gurl_android.h"

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxPedal(
    int id,
    std::u16string hint,
    std::u16string suggestion_contents,
    std::u16string accessibility_suffix,
    std::u16string accessibility_hint,
    GURL url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ScopedJavaGlobalRef(Java_OmniboxPedal_build(
      env, id, base::android::ConvertUTF16ToJavaString(env, hint),
      base::android::ConvertUTF16ToJavaString(env, suggestion_contents),
      base::android::ConvertUTF16ToJavaString(env, accessibility_suffix),
      base::android::ConvertUTF16ToJavaString(env, accessibility_hint),
      url::GURLAndroid::FromNativeGURL(env, url)));
}

base::android::ScopedJavaGlobalRef<jobject> BuildHistoryClustersAction(
    int id,
    std::u16string hint,
    std::u16string suggestion_contents,
    std::u16string accessibility_suffix,
    std::u16string accessibility_hint,
    GURL url,
    std::string query) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ScopedJavaGlobalRef(Java_HistoryClustersAction_build(
      env, id, base::android::ConvertUTF16ToJavaString(env, hint),
      base::android::ConvertUTF16ToJavaString(env, suggestion_contents),
      base::android::ConvertUTF16ToJavaString(env, accessibility_suffix),
      base::android::ConvertUTF16ToJavaString(env, accessibility_hint),
      url::GURLAndroid::FromNativeGURL(env, url),
      base::android::ConvertUTF8ToJavaString(env, query)));
}
