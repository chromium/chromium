// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/android/crash_keys_android.h"

#include <array>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/crash/core/common/crash_key.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/crash/android/jni_headers/CrashKeys_jni.h"

namespace {

using JavaCrashKey = crash_reporter::CrashKeyString<64>;
using BigJavaCrashKey = crash_reporter::CrashKeyString<256>;

BigJavaCrashKey g_installed_modules_key("installed_modules");

JavaCrashKey& GetCrashKey(jint index) {
  // See CrashKeys.java for how to add a new crash key.
  static std::array<JavaCrashKey,
                    static_cast<size_t>(CrashKeyIndex::NUM_SMALL_KEYS)>
      crash_keys{{
          {"application_status", JavaCrashKey::Tag::kArray},
          {"partner_customization_config", JavaCrashKey::Tag::kArray},
          {"first_run", JavaCrashKey::Tag::kArray},
      }};
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
    if (key == static_cast<jint>(CrashKeyIndex::INSTALLED_MODULES)) {
      g_installed_modules_key.Clear();
    } else {
      GetCrashKey(key).Clear();
    }
  } else {
    std::string val = base::android::ConvertJavaStringToUTF8(env, value);
    if (key == static_cast<jint>(CrashKeyIndex::INSTALLED_MODULES)) {
      g_installed_modules_key.Set(val);
    } else {
      GetCrashKey(key).Set(val);
    }
  }
}
