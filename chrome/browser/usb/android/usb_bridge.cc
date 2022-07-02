// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/android/jni_headers/UsbBridge_jni.h"
#include "chrome/browser/usb/usb_tab_helper.h"
#include "content/public/browser/web_contents.h"

jboolean JNI_UsbBridge_IsWebContentsConnectedToUsbDevice(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);

  UsbTabHelper* usb_tab_helper = UsbTabHelper::FromWebContents(web_contents);
  return (usb_tab_helper && usb_tab_helper->IsDeviceConnected());
}
