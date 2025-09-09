// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_ANDROID_TIMESTAMPED_PREF_VALUE_BRIDGE_ANDROID_H_
#define COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_ANDROID_TIMESTAMPED_PREF_VALUE_BRIDGE_ANDROID_H_

#include "base/android/scoped_java_ref.h"

namespace sync_preferences {

struct TimestampedPrefValue;

// Bridge for converting a native TimestampedPrefValue to a Java
// TimestampedPrefValue.
//
// The types which are supported for conversion are Boolean, Double, Integer,
// and String.
class TimestampedPrefValueBridge {
 public:
  explicit TimestampedPrefValueBridge(const TimestampedPrefValue& input);
  ~TimestampedPrefValueBridge();

  TimestampedPrefValueBridge(const TimestampedPrefValueBridge&) = delete;
  TimestampedPrefValueBridge& operator=(const TimestampedPrefValueBridge&) =
      delete;

  // Returns a local reference to the Java object.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() const;

 private:
  // The Java counterpart, owned by this C++ object.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_CROSS_DEVICE_PREF_TRACKER_ANDROID_TIMESTAMPED_PREF_VALUE_BRIDGE_ANDROID_H_
