// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/split_tabs/android/split_tab_id_jni_conversion.h"

#include "base/android/token_android.h"
#include "base/token.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/split_tabs/split_tabs_jni_headers/SplitTabId_jni.h"
#include "components/tab_groups/tab_groups_jni_headers/TokenId_jni.h"

namespace jni_zero {

template <>
split_tabs::SplitTabId FromJniType<split_tabs::SplitTabId>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  return split_tabs::SplitTabId::FromRawToken(
      tab_groups::Java_TokenId_getToken(env, j_object));
}

template <>
ScopedJavaLocalRef<jobject> ToJniType<split_tabs::SplitTabId>(
    JNIEnv* env,
    const split_tabs::SplitTabId& split_tab_id) {
  return split_tabs::Java_SplitTabId_fromRawToken(env, split_tab_id.token());
}

}  // namespace jni_zero
