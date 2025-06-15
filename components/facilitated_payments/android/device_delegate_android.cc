// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/device_delegate_android.h"

#include "base/android/jni_android.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/facilitated_payments/android/java/jni_headers/DeviceDelegate_jni.h"

namespace payments::facilitated {

bool IsWalletEligibleForPixAccountLinking() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_DeviceDelegate_isWalletEligibleForPixAccountLinking(env);
}

void OpenPixAccountLinkingPageInWallet(content::WebContents* web_contents) {
  if (!web_contents || !web_contents->GetNativeView() ||
      !web_contents->GetNativeView()->GetWindowAndroid()) {
    // TODO(crbug.com/419108993): Log metrics.
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DeviceDelegate_openPixAccountLinkingPageInWallet(
      env, web_contents->GetTopLevelNativeWindow()->GetJavaObject());
}

}  // namespace payments::facilitated
