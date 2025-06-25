// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/device_delegate_android.h"

#include "base/android/application_status_listener.h"
#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/facilitated_payments/android/java/jni_headers/DeviceDelegate_jni.h"

namespace payments::facilitated {

DeviceDelegateAndroid::DeviceDelegateAndroid(content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()) {
  app_status_listener_ = base::android::ApplicationStatusListener::New(
      base::BindRepeating(&DeviceDelegateAndroid::OnApplicationStateChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

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

void DeviceDelegateAndroid::SetOnReturnToChromeCallback(
    base::OnceClosure callback) {
  on_return_to_chrome_callback_ = std::move(callback);
}

void DeviceDelegateAndroid::OnApplicationStateChanged(
    base::android::ApplicationState state) {
  // If there's no active callback, there's no need to track app state.
  if (!on_return_to_chrome_callback_) {
    return;
  }
  // Chrome app is moved to the background.
  if (state == base::android::ApplicationState::
                   APPLICATION_STATE_HAS_STOPPED_ACTIVITIES) {
    is_chrome_in_background_ = true;
    // Chrome app is moved to the foreground.
  } else if (state == base::android::ApplicationState::
                          APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
    // The callback is run only if Chrome was moved to background before coming
    // back to the foreground.
    if (!is_chrome_in_background_) {
      return;
    }
    is_chrome_in_background_ = false;
    std::move(on_return_to_chrome_callback_).Run();
  }
}

}  // namespace payments::facilitated
