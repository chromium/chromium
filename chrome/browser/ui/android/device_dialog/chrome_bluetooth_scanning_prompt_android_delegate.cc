// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/device_dialog/chrome_bluetooth_scanning_prompt_android_delegate.h"

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/security_state/content/security_state_tab_helper.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChromeBluetoothScanningPromptAndroidDelegate_jni.h"

ChromeBluetoothScanningPromptAndroidDelegate::
    ChromeBluetoothScanningPromptAndroidDelegate(Profile* profile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_delegate_.Reset(
      Java_ChromeBluetoothScanningPromptAndroidDelegate_Constructor(
          env, profile->GetJavaObject()));
}

ChromeBluetoothScanningPromptAndroidDelegate::
    ~ChromeBluetoothScanningPromptAndroidDelegate() = default;

base::android::ScopedJavaLocalRef<jobject>
ChromeBluetoothScanningPromptAndroidDelegate::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_delegate_);
}

security_state::SecurityLevel
ChromeBluetoothScanningPromptAndroidDelegate::GetSecurityLevel(
    content::WebContents* web_contents) {
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);
  return helper->GetSecurityLevel();
}
