// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check.h"
#include "components/origin_matcher/origin_matcher.h"
#include "third_party/jni_zero/jni_zero.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/origin_matcher/android/jni/OriginMatcher_jni.h"

namespace origin_matcher {

OriginMatcher ToNativeOriginMatcher(JNIEnv* env,
                                    const jni_zero::JavaRef<jobject>& jobject) {
  // Copy the native origin matcher the java object holds onto.
  // The Java object is expected to clean up its own origin matcher's
  // reference.
  OriginMatcher* native = Java_OriginMatcher_getNative(env, jobject);
  CHECK(native);
  return *native;
}

static bool JNI_OriginMatcher_Matches(JNIEnv* env,
                                      OriginMatcher* matcher,
                                      const url::Origin& origin) {
  return matcher->Matches(origin);
}

static std::vector<std::string> JNI_OriginMatcher_SetRuleList(
    JNIEnv* env,
    OriginMatcher* matcher,
    const std::vector<std::string>& rules) {
  OriginMatcher new_rules_matcher;
  std::vector<std::string> bad_rules;

  for (std::string rule : rules) {
    if (!new_rules_matcher.AddRuleFromString(rule)) {
      bad_rules.push_back(rule);
    }
  }

  if (bad_rules.empty()) {
    matcher->MoveRules(new_rules_matcher);
  }

  return bad_rules;
}

static std::vector<std::string> JNI_OriginMatcher_Serialize(
    JNIEnv* env,
    OriginMatcher* matcher) {
  return matcher->Serialize();
}

static jni_zero::JniUniquePtr<OriginMatcher> JNI_OriginMatcher_Create(
    JNIEnv* env) {
  return jni_zero::MakeUnique<OriginMatcher>();
}
}  // namespace origin_matcher

DEFINE_JNI(OriginMatcher)
