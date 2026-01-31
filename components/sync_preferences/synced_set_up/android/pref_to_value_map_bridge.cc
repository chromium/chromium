// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/synced_set_up/android/pref_to_value_map_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/sync_preferences/synced_set_up/android/jni_headers/PrefToValueMapBridge_jni.h"

namespace sync_preferences::synced_set_up {

static int64_t JNI_PrefToValueMapBridge_Init(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_caller) {
  return reinterpret_cast<intptr_t>(new PrefToValueMapBridge(j_caller));
}

PrefToValueMapBridge::PrefToValueMapBridge(
    const base::android::JavaRef<jobject>& j_obj)
    : jobj_(j_obj) {}

PrefToValueMapBridge::~PrefToValueMapBridge() = default;

void PrefToValueMapBridge::PutStringValue(JNIEnv* env,
                                          const std::string_view& key,
                                          const std::string_view& value) {
  Java_PrefToValueMapBridge_putStringValue(env, jobj_, key.data(),
                                           value.data());
}

void PrefToValueMapBridge::PutIntValue(JNIEnv* env,
                                       const std::string_view& key,
                                       int value) {
  Java_PrefToValueMapBridge_putIntValue(env, jobj_, key.data(), value);
}

void PrefToValueMapBridge::PutBooleanValue(JNIEnv* env,
                                           const std::string_view& key,
                                           bool value) {
  Java_PrefToValueMapBridge_putBooleanValue(env, jobj_, key.data(), value);
}

void PrefToValueMapBridge::Destroy(JNIEnv* env) {
  delete this;
}

}  // namespace sync_preferences::synced_set_up

DEFINE_JNI(PrefToValueMapBridge)
