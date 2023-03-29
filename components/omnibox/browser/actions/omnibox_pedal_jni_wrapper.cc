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

namespace {
// The following cannot be generated with jni_headers - no native methods in
// base class.
const char kOmniboxActionClass[] =
    "org/chromium/components/omnibox/action/OmniboxAction";
JNI_REGISTRATION_EXPORT std::atomic<jclass>
    g_org_chromium_components_omnibox_action_OmniboxAction_clazz(nullptr);
inline jclass org_chromium_components_omnibox_action_OmniboxAction_clazz(
    JNIEnv* env) {
  return base::android::LazyGetClass(
      env, kOmniboxActionClass,
      &g_org_chromium_components_omnibox_action_OmniboxAction_clazz);
}
}  // namespace

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxPedal(
    std::u16string hint,
    OmniboxPedalId pedal_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ScopedJavaGlobalRef(Java_OmniboxPedal_Constructor(
      env, base::android::ConvertUTF16ToJavaString(env, hint),
      static_cast<int32_t>(pedal_id)));
}

base::android::ScopedJavaGlobalRef<jobject> BuildHistoryClustersAction(
    std::u16string hint,
    std::string query) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return base::android::ScopedJavaGlobalRef(
      Java_HistoryClustersAction_Constructor(
          env, base::android::ConvertUTF16ToJavaString(env, hint),
          base::android::ConvertUTF8ToJavaString(env, query)));
}

// Convert a vector of OmniboxActions to Java counterpart.
base::android::ScopedJavaLocalRef<jobjectArray> ToJavaOmniboxActionsList(
    JNIEnv* env,
    const std::vector<scoped_refptr<OmniboxAction>>& actions) {
  jclass clazz =
      org_chromium_components_omnibox_action_OmniboxAction_clazz(env);
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
