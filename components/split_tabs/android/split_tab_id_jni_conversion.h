// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPLIT_TABS_ANDROID_SPLIT_TAB_ID_JNI_CONVERSION_H_
#define COMPONENTS_SPLIT_TABS_ANDROID_SPLIT_TAB_ID_JNI_CONVERSION_H_

#include "components/split_tabs/split_tab_id.h"
#include "third_party/jni_zero/jni_zero.h"

namespace jni_zero {

template <>
split_tabs::SplitTabId FromJniType<split_tabs::SplitTabId>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object);

template <>
ScopedJavaLocalRef<jobject> ToJniType<split_tabs::SplitTabId>(
    JNIEnv* env,
    const split_tabs::SplitTabId& split_tab_id);

}  // namespace jni_zero

#endif  // COMPONENTS_SPLIT_TABS_ANDROID_SPLIT_TAB_ID_JNI_CONVERSION_H_
