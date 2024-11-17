// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/android/tab_group_sync_conversions_utils.h"

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/public/types.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace tab_groups {
namespace {

// Invalid IDs are represented as -1 in the JNI bridge.
int kInvalidTabId = -1;

}  // namespace

LocalTabID FromJavaTabId(int tab_id) {
  return tab_id;
}

jint ToJavaTabId(const std::optional<LocalTabID>& tab_id) {
  return tab_id.value_or(kInvalidTabId);
}

ScopedJavaLocalRef<jstring> UuidToJavaString(JNIEnv* env,
                                             const base::Uuid& uuid) {
  return ConvertUTF8ToJavaString(env, uuid.AsLowercaseString());
}

base::Uuid JavaStringToUuid(JNIEnv* env, const JavaParamRef<jstring>& j_uuid) {
  return base::Uuid::ParseLowercase(ConvertJavaStringToUTF8(env, j_uuid));
}

}  // namespace tab_groups
