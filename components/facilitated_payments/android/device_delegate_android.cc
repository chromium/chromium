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

DeviceDelegateAndroid::DeviceDelegateAndroid(content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()) {}

DeviceDelegateAndroid::~DeviceDelegateAndroid() = default;

bool DeviceDelegateAndroid::IsPixAccountLinkingSupported() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_DeviceDelegate_isWalletEligibleForPixAccountLinking(env);
}

void DeviceDelegateAndroid::LaunchPixAccountLinkingPage() {
  if (!web_contents_ || !web_contents_->GetNativeView() ||
      !web_contents_->GetNativeView()->GetWindowAndroid()) {
    // TODO(crbug.com/419108993): Log metrics.
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DeviceDelegate_openPixAccountLinkingPageInWallet(
      env, web_contents_->GetTopLevelNativeWindow()->GetJavaObject());
}

}  // namespace payments::facilitated
