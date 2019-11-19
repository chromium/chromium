// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/android/crash_keys_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/crash/android/jni_headers/CrashKeys_jni.h"
#include "components/crash/core/common/crash_key.h"

namespace {

using JavaCrashKey = crash_reporter::CrashKeyString<64>;

JavaCrashKey& GetCrashKey(int index) {
  // See CrashKeys.java for how to add a new crash key.
  static JavaCrashKey crash_keys[] = {
      {"loaded_dynamic_module", JavaCrashKey::Tag::kArray},
      {"active_dynamic_module", JavaCrashKey::Tag::kArray},
      {"application_status", JavaCrashKey::Tag::kArray},
      {"installed_modules", JavaCrashKey::Tag::kArray},
      {"emulated_modules", JavaCrashKey::Tag::kArray},
      {"dynamic_module_dex_name", JavaCrashKey::Tag::kArray},
  };
  static_assert(
      base::size(crash_keys) == static_cast<size_t>(CrashKeyIndex::NUM_ENTRIES),
      "crash_keys out of sync with index enum");

  return crash_keys[index];
}

}  // namespace

void SetAndroidCrashKey(CrashKeyIndex index, const std::string& value) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jinstance =
      Java_CrashKeys_getInstance(env);
  Java_CrashKeys_set(env, jinstance, static_cast<jint>(index),
                     base::android::ConvertUTF8ToJavaString(env, value));
}

void ClearAndroidCrashKey(CrashKeyIndex index) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jinstance =
      Java_CrashKeys_getInstance(env);
  Java_CrashKeys_set(env, jinstance, static_cast<jint>(index), nullptr);
}

void FlushAndroidCrashKeys() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jinstance =
      Java_CrashKeys_getInstance(env);
  Java_CrashKeys_flushToNative(env, jinstance);
}

static void JNI_CrashKeys_Set(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint key,
    const base::android::JavaParamRef<jstring>& value) {
  if (value.is_null()) {
    GetCrashKey(key).Clear();
  } else {
    GetCrashKey(key).Set(base::android::ConvertJavaStringToUTF8(env, value));
  }
}
