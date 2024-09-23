// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/usb/android/jni_headers/UsbBridge_jni.h"

jboolean JNI_UsbBridge_IsWebContentsConnectedToUsbDevice(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  return content::WebContents::FromJavaWebContents(java_web_contents)
      ->IsConnectedToUsbDevice();
}
