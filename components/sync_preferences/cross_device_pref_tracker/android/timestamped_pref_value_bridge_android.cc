// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_preferences/cross_device_pref_tracker/android/timestamped_pref_value_bridge_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/notreached.h"
#include "base/values.h"
#include "components/sync_preferences/cross_device_pref_tracker/android/jni_headers/TimestampedPrefValue_jni.h"
#include "components/sync_preferences/cross_device_pref_tracker/timestamped_pref_value.h"

namespace sync_preferences {

namespace {

// Helper to create a local ref to a new Java TimestampedPrefValue object.
base::android::ScopedJavaLocalRef<jobject> CreateJavaObject(
    JNIEnv* env,
    const TimestampedPrefValue& input) {
  long last_observed_change_time_ms =
      input.last_observed_change_time.InMillisecondsSinceUnixEpoch();

  switch (input.value.type()) {
    case base::Value::Type::BOOLEAN:
      return Java_TimestampedPrefValue_createBooleanPrefValue(
          env, input.value.GetBool(), last_observed_change_time_ms,
          input.device_sync_cache_guid);
    case base::Value::Type::DOUBLE:
      return Java_TimestampedPrefValue_createDoublePrefValue(
          env, input.value.GetDouble(), last_observed_change_time_ms,
          input.device_sync_cache_guid);
    case base::Value::Type::INTEGER:
      return Java_TimestampedPrefValue_createIntegerPrefValue(
          env, input.value.GetInt(), last_observed_change_time_ms,
          input.device_sync_cache_guid);
    case base::Value::Type::STRING:
      return Java_TimestampedPrefValue_createStringPrefValue(
          env, input.value.GetString(), last_observed_change_time_ms,
          input.device_sync_cache_guid);
    case base::Value::Type::BINARY:
      // TODO(crbug.com/433719441): Implement this if needed.
      NOTREACHED() << "Converting TimestampedPrefValue with Binary value is "
                      "not yet supported";
    case base::Value::Type::DICT:
      // TODO(crbug.com/433719441): Implement this if needed.
      NOTREACHED() << "Converting TimestampedPrefValue with Dict value is not "
                      "yet supported";
    case base::Value::Type::LIST:
      // TODO(crbug.com/433719441): Implement this if needed.
      NOTREACHED() << "Converting TimestampedPrefValue with List value is not "
                      "yet supported";
    case base::Value::Type::NONE:
      NOTREACHED() << "Value Type should not be NONE";
  }
}

}  // namespace

TimestampedPrefValueBridge::TimestampedPrefValueBridge(
    const TimestampedPrefValue& input) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_ref_.Reset(CreateJavaObject(env, input));
}

TimestampedPrefValueBridge::~TimestampedPrefValueBridge() = default;

base::android::ScopedJavaLocalRef<jobject>
TimestampedPrefValueBridge::GetJavaObject() const {
  return base::android::ScopedJavaLocalRef<jobject>(java_ref_);
}

}  // namespace sync_preferences

DEFINE_JNI(TimestampedPrefValue)
