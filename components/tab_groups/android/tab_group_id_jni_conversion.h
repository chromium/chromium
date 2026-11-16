// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TAB_GROUPS_ANDROID_TAB_GROUP_ID_JNI_CONVERSION_H_
#define COMPONENTS_TAB_GROUPS_ANDROID_TAB_GROUP_ID_JNI_CONVERSION_H_

#include "base/component_export.h"
#include "components/tab_groups/tab_group_id.h"
#include "third_party/jni_zero/jni_zero.h"

namespace jni_zero {

template <>
COMPONENT_EXPORT(TAB_GROUPS)
tab_groups::TabGroupId
    FromJniType<tab_groups::TabGroupId>(JNIEnv* env,
                                        const JavaRef<jobject>& j_object);

template <>
COMPONENT_EXPORT(TAB_GROUPS)
ScopedJavaLocalRef<jobject> ToJniType<tab_groups::TabGroupId>(
    JNIEnv* env,
    const tab_groups::TabGroupId& tab_group_id);

}  // namespace jni_zero

#endif  // COMPONENTS_TAB_GROUPS_ANDROID_TAB_GROUP_ID_JNI_CONVERSION_H_
