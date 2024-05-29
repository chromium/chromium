// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_action_factory_android.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/lazy_instance.h"
#include "omnibox_action.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/omnibox/browser/jni_headers/OmniboxActionFactory_jni.h"

namespace {

base::LazyInstance<base::android::ScopedJavaGlobalRef<jobject>>::
    DestructorAtExit g_java_factory = LAZY_INSTANCE_INITIALIZER;
}  // namespace

/* static */ void JNI_OmniboxActionFactory_SetFactory(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& factory) {
  if (factory) {
    g_java_factory.Get().Reset(factory);
  } else {
    g_java_factory.Get().Reset(nullptr);
  }
}

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxPedal(
    JNIEnv* env,
    intptr_t instance,
    const std::u16string& hint,
    const std::u16string& accessibility_hint,
    OmniboxPedalId pedal_id) {
  return base::android::ScopedJavaGlobalRef<jobject>(
      Java_OmniboxActionFactory_buildOmniboxPedal(
          env, g_java_factory.Get(), instance,
          base::android::ConvertUTF16ToJavaString(env, hint),
          base::android::ConvertUTF16ToJavaString(env, accessibility_hint),
          static_cast<int32_t>(pedal_id)));
}

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxActionInSuggest(
    JNIEnv* env,
    intptr_t instance,
    const std::u16string& hint,
    const std::u16string& accessibility_hint,
    int action_type,
    const std::string& action_uri) {
  return base::android::ScopedJavaGlobalRef<jobject>(
      Java_OmniboxActionFactory_buildActionInSuggest(
          env, g_java_factory.Get(), instance,
          base::android::ConvertUTF16ToJavaString(env, hint),
          base::android::ConvertUTF16ToJavaString(env, accessibility_hint),
          action_type,
          base::android::ConvertUTF8ToJavaString(env, action_uri)));
}

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxAnswerAction(
    JNIEnv* env,
    intptr_t instance,
    const std::u16string& hint,
    const std::u16string& accessibility_hint) {
  return base::android::ScopedJavaGlobalRef<jobject>(
      Java_OmniboxActionFactory_buildOmniboxAnswerAction(
          env, g_java_factory.Get(), instance,
          base::android::ConvertUTF16ToJavaString(env, hint),
          base::android::ConvertUTF16ToJavaString(env, accessibility_hint)));
}

// Convert a vector of OmniboxActions to Java counterpart.
std::vector<jni_zero::ScopedJavaLocalRef<jobject>> ToJavaOmniboxActionsList(
    JNIEnv* env,
    const std::vector<scoped_refptr<OmniboxAction>>& actions) {
  std::vector<base::android::ScopedJavaLocalRef<jobject>> ret;
  // Early return for cases where Action creation is not yet possible, e.g.
  // if the control is passed from the IntentHandler.
  if (!g_java_factory.IsCreated() || !g_java_factory.Get()) {
    return {};
  }

  for (const auto& action : actions) {
    auto jobj = action->GetOrCreateJavaObject(env);
    if (jobj) {
      ret.emplace_back(std::move(jobj));
    }
  }

  // Return only after all actions are created to capture cases where some
  // actions were found, but none was applicable to Android.
  return ret;
}
