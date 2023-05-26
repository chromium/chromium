// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_action_factory_android.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/lazy_instance.h"
#include "components/omnibox/browser/jni_headers/OmniboxActionFactory_jni.h"
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

base::LazyInstance<base::android::ScopedJavaGlobalRef<jobject>>::
    DestructorAtExit g_java_factory = LAZY_INSTANCE_INITIALIZER;
}  // namespace

/* static */ void JNI_OmniboxActionFactory_SetFactory(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& factory) {
  g_java_factory.Get().Reset(factory);
}

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxPedal(
    JNIEnv* env,
    intptr_t instance,
    const std::u16string& hint,
    OmniboxPedalId pedal_id) {
  return base::android::ScopedJavaGlobalRef(
      Java_OmniboxActionFactory_buildOmniboxPedal(
          env, g_java_factory.Get(), instance,
          base::android::ConvertUTF16ToJavaString(env, hint),
          static_cast<int32_t>(pedal_id)));
}

base::android::ScopedJavaGlobalRef<jobject> BuildHistoryClustersAction(
    JNIEnv* env,
    intptr_t instance,
    const std::u16string& hint,
    const std::string& query) {
  return base::android::ScopedJavaGlobalRef(
      Java_OmniboxActionFactory_buildHistoryClustersAction(
          env, g_java_factory.Get(), instance,
          base::android::ConvertUTF16ToJavaString(env, hint),
          base::android::ConvertUTF8ToJavaString(env, query)));
}

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxActionInSuggest(
    JNIEnv* env,
    intptr_t instance,
    const std::u16string& hint,
    int action_type,
    const std::string& action_uri) {
  return base::android::ScopedJavaGlobalRef(
      Java_OmniboxActionFactory_buildActionInSuggest(
          env, g_java_factory.Get(), instance,
          base::android::ConvertUTF16ToJavaString(env, hint), action_type,
          base::android::ConvertUTF8ToJavaString(env, action_uri)));
}

// Convert a vector of OmniboxActions to Java counterpart.
base::android::ScopedJavaLocalRef<jobjectArray> ToJavaOmniboxActionsList(
    JNIEnv* env,
    const std::vector<scoped_refptr<OmniboxAction>>& actions) {
  jclass clazz =
      org_chromium_components_omnibox_action_OmniboxAction_clazz(env);
  // Fires if OmniboxAction is not part of this build target.
  DCHECK(clazz);
  base::android::ScopedJavaLocalRef<jobjectArray> jactions(
      env, env->NewObjectArray(actions.size(), clazz, nullptr));
  base::android::CheckException(env);

  for (size_t index = 0; index < actions.size(); index++) {
    auto jobj = actions[index]->GetOrCreateJavaObject(env);
    if (jobj) {
      env->SetObjectArrayElement(jactions.obj(), index, jobj.obj());
    }
  }

  return jactions;
}
