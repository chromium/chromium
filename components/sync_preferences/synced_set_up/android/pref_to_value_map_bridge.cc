// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/synced_set_up/android/pref_to_value_map_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/sync_preferences/synced_set_up/android/jni_headers/PrefToValueMapBridge_jni.h"

namespace sync_preferences::synced_set_up {

static jni_zero::JniUniquePtr<PrefToValueMapBridge>
JNI_PrefToValueMapBridge_Init(JNIEnv* env,
                              const base::android::JavaRef<jobject>& j_caller) {
  return jni_zero::MakeUnique<PrefToValueMapBridge>(env, j_caller);
}

PrefToValueMapBridge::PrefToValueMapBridge(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_obj)
    : jobj_(env, j_obj) {}

PrefToValueMapBridge::~PrefToValueMapBridge() = default;

void PrefToValueMapBridge::PutStringValue(JNIEnv* env,
                                          std::string_view key,
                                          std::string_view value) {
  Java_PrefToValueMapBridge_putStringValue(env, GetJavaObject(env), key, value);
}

void PrefToValueMapBridge::PutIntValue(JNIEnv* env,
                                       std::string_view key,
                                       int value) {
  Java_PrefToValueMapBridge_putIntValue(env, GetJavaObject(env), key, value);
}

void PrefToValueMapBridge::PutBooleanValue(JNIEnv* env,
                                           std::string_view key,
                                           bool value) {
  Java_PrefToValueMapBridge_putBooleanValue(env, GetJavaObject(env), key,
                                            value);
}

base::android::ScopedJavaLocalRef<jobject> PrefToValueMapBridge::GetJavaObject(
    JNIEnv* env) const {
  base::android::ScopedJavaLocalRef<jobject> obj = jobj_.get(env);
  CHECK(obj);
  return obj;
}

}  // namespace sync_preferences::synced_set_up

DEFINE_JNI(PrefToValueMapBridge)
