// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/origin_matcher/origin_matcher.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/origin_matcher/android/jni/OriginMatcher_jni.h"

namespace origin_matcher {
namespace {
OriginMatcher* FromPtr(long ptr) {
  return reinterpret_cast<OriginMatcher*>(ptr);
}
}  // namespace

OriginMatcher ToNativeOriginMatcher(JNIEnv* env,
                                    const jni_zero::JavaRef<jobject>& jobject) {
  // Copy the native origin matcher the java object holds onto.
  // The Java object is expected to clean up its own origin matcher's
  // reference.
  OriginMatcher origin_matcher = *reinterpret_cast<OriginMatcher*>(
      Java_OriginMatcher_getNative(env, jobject));
  return origin_matcher;
}

static bool JNI_OriginMatcher_Matches(JNIEnv* env,
                                      jlong ptr,
                                      url::Origin& origin) {
  auto* matcher = FromPtr(ptr);
  return matcher->Matches(origin);
}

static std::vector<std::string> JNI_OriginMatcher_SetRuleList(
    JNIEnv* env,
    jlong ptr,
    std::vector<std::string>& rules) {
  auto* matcher = FromPtr(ptr);

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

static std::vector<std::string> JNI_OriginMatcher_Serialize(JNIEnv* env,
                                                            jlong ptr) {
  auto* matcher = FromPtr(ptr);
  return matcher->Serialize();
}

static void JNI_OriginMatcher_Destroy(JNIEnv* env, jlong ptr) {
  delete FromPtr(ptr);
}

static jlong JNI_OriginMatcher_Create(JNIEnv* env) {
  OriginMatcher* matcher = new OriginMatcher();
  return reinterpret_cast<intptr_t>(matcher);
}
}  // namespace origin_matcher

DEFINE_JNI(OriginMatcher)
