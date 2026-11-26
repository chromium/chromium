// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_SYNCED_SET_UP_ANDROID_PREF_TO_VALUE_MAP_BRIDGE_H_
#define COMPONENTS_SYNC_PREFERENCES_SYNCED_SET_UP_ANDROID_PREF_TO_VALUE_MAP_BRIDGE_H_

#include <string_view>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"

namespace sync_preferences::synced_set_up {

class PrefToValueMapBridge {
 public:
  PrefToValueMapBridge(JNIEnv* env,
                       const base::android::JavaRef<jobject>& j_obj);
  PrefToValueMapBridge(const PrefToValueMapBridge&) = delete;
  PrefToValueMapBridge& operator=(const PrefToValueMapBridge&) = delete;
  ~PrefToValueMapBridge();

  void PutStringValue(JNIEnv* env,
                      std::string_view key,
                      std::string_view value);
  void PutIntValue(JNIEnv* env, std::string_view key, int value);
  void PutBooleanValue(JNIEnv* env, std::string_view key, bool value);

 private:
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject(JNIEnv* env) const;

  JavaObjectWeakGlobalRef jobj_;
};

}  // namespace sync_preferences::synced_set_up

#endif  // COMPONENTS_SYNC_PREFERENCES_SYNCED_SET_UP_ANDROID_PREF_TO_VALUE_MAP_BRIDGE_H_
