// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_ANDROID_JNI_CONVERSION_H_
#define COMPONENTS_TABS_PUBLIC_ANDROID_JNI_CONVERSION_H_

#include "base/android/jni_android.h"
#include "components/tabs/public/android/jni_headers/TabStripCollection_jni.h"
#include "components/tabs/public/tab_strip_collection.h"
#include "third_party/jni_zero/jni_zero.h"

// Android specific helpers for converting between C++ and Java types.

namespace jni_zero {

template <>
inline tabs::TabStripCollection* FromJniType<tabs::TabStripCollection*>(
    JNIEnv* env,
    const JavaRef<jobject>& input) {
  return reinterpret_cast<tabs::TabStripCollection*>(
      Java_TabStripCollection_getNativePtr(env, input));
}

template <>
inline base::android::ScopedJavaLocalRef<jobject>
ToJniType<tabs::TabStripCollection>(JNIEnv* env,
                                    const tabs::TabStripCollection& input) {
  return Java_TabStripCollection_Constructor(env,
                                             reinterpret_cast<jlong>(&input));
}

}  // namespace jni_zero

#endif  // COMPONENTS_TABS_PUBLIC_ANDROID_JNI_CONVERSION_H_

DEFINE_JNI(TabStripCollection)
