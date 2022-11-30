// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/announcement_notification/announcement_notification_delegate_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/AnnouncementNotificationManager_jni.h"
#include "chrome/browser/profiles/profile.h"          // nogncheck
#include "chrome/browser/profiles/profile_android.h"  // nogncheck
#include "chrome/browser/updates/announcement_notification/announcement_notification_service_factory.h"

AnnouncementNotificationDelegateAndroid::
    AnnouncementNotificationDelegateAndroid() = default;

AnnouncementNotificationDelegateAndroid::
    ~AnnouncementNotificationDelegateAndroid() = default;

void AnnouncementNotificationDelegateAndroid::ShowNotification() {
  auto* env = base::android::AttachCurrentThread();
  GURL url = AnnouncementNotificationService::GetAnnouncementURL();
  Java_AnnouncementNotificationManager_showNotification(
      env, base::android::ConvertUTF8ToJavaString(env, url.spec()));
}

bool AnnouncementNotificationDelegateAndroid::IsFirstRun() {
  return Java_AnnouncementNotificationManager_isFirstRun(
      base::android::AttachCurrentThread());
}
