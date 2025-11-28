// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/safety_hub/notification_wrapper_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/safety_hub/android/jni_headers/UnsubscribedNotificationsNotificationManager_jni.h"

NotificationWrapperAndroid::~NotificationWrapperAndroid() = default;

void NotificationWrapperAndroid::DisplayNotification(
    int num_revoked_permissions) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UnsubscribedNotificationsNotificationManager_displayNotification(
      env, num_revoked_permissions);
}

void NotificationWrapperAndroid::UpdateNotification(
    int num_revoked_permissions) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_UnsubscribedNotificationsNotificationManager_updateNotification(
      env, num_revoked_permissions);
}

DEFINE_JNI(UnsubscribedNotificationsNotificationManager)
