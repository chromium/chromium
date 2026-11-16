// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/split_tabs/android/split_tab_visual_data_jni_conversion.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/split_tabs/split_tabs_jni_headers/SplitTabVisualData_jni.h"

namespace jni_zero {

template <>
split_tabs::SplitTabVisualData FromJniType<split_tabs::SplitTabVisualData>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  split_tabs::SplitTabLayout split_layout =
      split_tabs::Java_SplitTabVisualData_getSplitLayout(env, j_object);
  double split_ratio =
      split_tabs::Java_SplitTabVisualData_getSplitRatio(env, j_object);
  return split_tabs::SplitTabVisualData(split_layout, split_ratio);
}

template <>
ScopedJavaLocalRef<jobject> ToJniType<split_tabs::SplitTabVisualData>(
    JNIEnv* env,
    const split_tabs::SplitTabVisualData& visual_data) {
  return split_tabs::Java_SplitTabVisualData_Constructor(
      env, visual_data.split_layout(), visual_data.split_ratio());
}

}  // namespace jni_zero
