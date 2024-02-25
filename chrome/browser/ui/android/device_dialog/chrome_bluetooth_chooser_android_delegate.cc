// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/device_dialog/chrome_bluetooth_chooser_android_delegate.h"

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/ChromeBluetoothChooserAndroidDelegate_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"

ChromeBluetoothChooserAndroidDelegate::ChromeBluetoothChooserAndroidDelegate(
    Profile* profile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_delegate_.Reset(Java_ChromeBluetoothChooserAndroidDelegate_Constructor(
      env, ProfileAndroid::FromProfile(profile)->GetJavaObject()));
}

ChromeBluetoothChooserAndroidDelegate::
    ~ChromeBluetoothChooserAndroidDelegate() = default;

base::android::ScopedJavaLocalRef<jobject>
ChromeBluetoothChooserAndroidDelegate::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_delegate_);
}

security_state::SecurityLevel
ChromeBluetoothChooserAndroidDelegate::GetSecurityLevel(
    content::WebContents* web_contents) {
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);
  return helper->GetSecurityLevel();
}
