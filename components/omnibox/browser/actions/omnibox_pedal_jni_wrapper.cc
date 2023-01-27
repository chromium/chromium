// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_service.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/omnibox/browser/jni_headers/HistoryClustersAction_jni.h"
#include "components/omnibox/browser/jni_headers/OmniboxPedal_jni.h"
#include "omnibox_action.h"
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

// Convert a vector of OmniboxActions to Java counterpart.
base::android::ScopedJavaLocalRef<jobjectArray> ToJavaOmniboxActionsList(
    JNIEnv* env,
    const std::vector<scoped_refptr<OmniboxAction>>& actions) {
  jclass clazz = org_chromium_components_omnibox_action_OmniboxPedal_clazz(env);
  // Fires if OmniboxPedal is not part of this build target.
  DCHECK(clazz);
  base::android::ScopedJavaLocalRef<jobjectArray> jactions(
      env, env->NewObjectArray(actions.size(), clazz, nullptr));
  base::android::CheckException(env);

  for (size_t index = 0; index < actions.size(); index++) {
    env->SetObjectArrayElement(jactions.obj(), index,
                               actions[index]->GetJavaObject().obj());
  }

  return jactions;
}
