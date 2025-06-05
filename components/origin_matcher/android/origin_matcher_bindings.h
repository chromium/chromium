// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_MATCHER_ANDROID_ORIGIN_MATCHER_BINDINGS_H_
#define COMPONENTS_ORIGIN_MATCHER_ANDROID_ORIGIN_MATCHER_BINDINGS_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/origin_matcher/origin_matcher.h"

namespace origin_matcher {
OriginMatcher ToNativeOriginMatcher(JNIEnv* env,
                                    const jni_zero::JavaRef<jobject>& jobject);
}  // namespace origin_matcher

namespace jni_zero {
template <>
inline origin_matcher::OriginMatcher FromJniType(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj) {
  return origin_matcher::ToNativeOriginMatcher(env, obj);
}
}  // namespace jni_zero

#endif  // COMPONENTS_ORIGIN_MATCHER_ANDROID_ORIGIN_MATCHER_BINDINGS_H_
