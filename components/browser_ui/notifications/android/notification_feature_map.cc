// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/browser_ui/notifications/android/features.h"
#include "components/browser_ui/notifications/android/notification_jni_headers/NotificationFeatureMap_jni.h"

namespace browser_ui {

namespace {

// Array of features exposed through the Java NotificationsFeatureMap API.
// Entries in this array may either refer to features defined in
// components/browser_ui/notifications/android/features.h or in other
// locations in the code base (e.g. content_features.h).
const base::Feature* const kFeaturesExposedToJava[] = {
    &kCacheNotificationsEnabled,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_NotificationFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace browser_ui
