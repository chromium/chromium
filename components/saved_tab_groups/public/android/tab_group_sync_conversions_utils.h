// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_ANDROID_TAB_GROUP_SYNC_CONVERSIONS_UTILS_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_ANDROID_TAB_GROUP_SYNC_CONVERSIONS_UTILS_H_

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/types.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace tab_groups {

// Converts a Java local tab ID to its native representation.
LocalTabID FromJavaTabId(int tab_id);

// Converts a local tab ID in native to its Java counterpart. If tab ID isn't
// present, -1 will be returned.
jint ToJavaTabId(const std::optional<LocalTabID>& tab_id);

// Converts a base::Uuid to a Java string.
ScopedJavaLocalRef<jstring> UuidToJavaString(JNIEnv* env,
                                             const base::Uuid& uuid);

// Converts a Java string to base::Uuid.
base::Uuid JavaStringToUuid(JNIEnv* env, const JavaParamRef<jstring>& j_uuid);

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_ANDROID_TAB_GROUP_SYNC_CONVERSIONS_UTILS_H_
