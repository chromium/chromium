// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_SYNCED_SET_UP_ANDROID_PREF_TO_VALUE_MAP_BRIDGE_H_
#define COMPONENTS_SYNC_PREFERENCES_SYNCED_SET_UP_ANDROID_PREF_TO_VALUE_MAP_BRIDGE_H_

#include <string>

#include "base/android/jni_android.h"

namespace sync_preferences::synced_set_up {

class PrefToValueMapBridge {
 public:
  explicit PrefToValueMapBridge(const base::android::JavaRef<jobject>& j_obj);
  PrefToValueMapBridge(const PrefToValueMapBridge&) = delete;
  PrefToValueMapBridge& operator=(const PrefToValueMapBridge&) = delete;
  ~PrefToValueMapBridge();

  void PutStringValue(JNIEnv* env,
                      const std::string_view& key,
                      const std::string_view& value);
  void PutIntValue(JNIEnv* env, const std::string_view& key, int value);
  void PutBooleanValue(JNIEnv* env, const std::string_view& key, bool value);

  void Destroy(JNIEnv* env);

 private:
  base::android::ScopedJavaGlobalRef<jobject> jobj_;
};

}  // namespace sync_preferences::synced_set_up

#endif  // COMPONENTS_SYNC_PREFERENCES_SYNCED_SET_UP_ANDROID_PREF_TO_VALUE_MAP_BRIDGE_H_
