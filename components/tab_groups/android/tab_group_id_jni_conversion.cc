// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tab_groups/android/tab_group_id_jni_conversion.h"

#include "base/android/token_android.h"
#include "base/token.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/tab_groups/tab_groups_jni_headers/TabGroupId_jni.h"
#include "components/tab_groups/tab_groups_jni_headers/TokenId_jni.h"

namespace jni_zero {

template <>
COMPONENT_EXPORT(TAB_GROUPS)
tab_groups::TabGroupId
    FromJniType<tab_groups::TabGroupId>(JNIEnv* env,
                                        const JavaRef<jobject>& j_object) {
  return tab_groups::TabGroupId::FromRawToken(
      tab_groups::Java_TokenId_getToken(env, j_object));
}

template <>
COMPONENT_EXPORT(TAB_GROUPS)
ScopedJavaLocalRef<jobject> ToJniType<tab_groups::TabGroupId>(
    JNIEnv* env,
    const tab_groups::TabGroupId& tab_group_id) {
  return tab_groups::Java_TabGroupId_fromRawToken(env, tab_group_id.token());
}

}  // namespace jni_zero
