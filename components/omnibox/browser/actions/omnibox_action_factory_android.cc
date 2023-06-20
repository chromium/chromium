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

base::LazyInstance<base::android::ScopedJavaGlobalRef<jclass>>::DestructorAtExit
    g_java_omnibox_action = LAZY_INSTANCE_INITIALIZER;

base::LazyInstance<base::android::ScopedJavaGlobalRef<jobject>>::
    DestructorAtExit g_java_factory = LAZY_INSTANCE_INITIALIZER;
}  // namespace

/* static */ void JNI_OmniboxActionFactory_SetFactory(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& factory) {
  if (factory) {
    g_java_omnibox_action.Get().Reset(
        base::android::GetClass(env, kOmniboxActionClass));
    g_java_factory.Get().Reset(factory);
  } else {
    g_java_omnibox_action.Get().Reset(nullptr);
    g_java_factory.Get().Reset(nullptr);
  }
}

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxPedal(
    JNIEnv* env,
    const std::u16string& hint,
    OmniboxPedalId pedal_id) {
  return base::android::ScopedJavaGlobalRef(
      Java_OmniboxActionFactory_buildOmniboxPedal(
          env, g_java_factory.Get(),
          base::android::ConvertUTF16ToJavaString(env, hint),
          static_cast<int32_t>(pedal_id)));
}

base::android::ScopedJavaGlobalRef<jobject> BuildHistoryClustersAction(
    JNIEnv* env,
    const std::u16string& hint,
    const std::string& query) {
  return base::android::ScopedJavaGlobalRef(
      Java_OmniboxActionFactory_buildHistoryClustersAction(
          env, g_java_factory.Get(),
          base::android::ConvertUTF16ToJavaString(env, hint),
          base::android::ConvertUTF8ToJavaString(env, query)));
}

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxActionInSuggest(
    JNIEnv* env,
    const std::u16string& hint,
    int action_type,
    const std::string& action_uri) {
  return base::android::ScopedJavaGlobalRef(
      Java_OmniboxActionFactory_buildActionInSuggest(
          env, g_java_factory.Get(),
          base::android::ConvertUTF16ToJavaString(env, hint), action_type,
          base::android::ConvertUTF8ToJavaString(env, action_uri)));
}

// Convert a vector of OmniboxActions to Java counterpart.
base::android::ScopedJavaLocalRef<jobjectArray> ToJavaOmniboxActionsList(
    JNIEnv* env,
    const std::vector<scoped_refptr<OmniboxAction>>& actions) {
  // Early return for cases where Action creation is not yet possible, e.g.
  // if the control is passed from the IntentHandler.
  if (!g_java_omnibox_action.IsCreated() || !g_java_factory.IsCreated() ||
      !g_java_omnibox_action.Get() || !g_java_factory.Get()) {
    return {};
  }

  std::vector<base::android::ScopedJavaLocalRef<jobject>> jactions_vec;
  for (const auto& action : actions) {
    auto jobj = action->GetOrCreateJavaObject(env);
    if (jobj) {
      jactions_vec.emplace_back(std::move(jobj));
    }
  }

  // Return only after all actions are created to capture cases where some
  // actions were found, but none was applicable to Android.
  if (!jactions_vec.size()) {
    return {};
  }

  return base::android::ToTypedJavaArrayOfObjects(
      env, jactions_vec,
      base::android::ScopedJavaLocalRef<jclass>(g_java_omnibox_action.Get()));
}
