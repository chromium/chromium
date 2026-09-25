// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/payments/payments_window_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check_deref.h"
#include "chrome/browser/ui/android/autofill/payments/payments_window_delegate.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/browser/ui/android/autofill/internal/jni_headers/PaymentsWindowBridge_jni.h"

namespace autofill::payments {

PaymentsWindowBridge::PaymentsWindowBridge(
    PaymentsWindowDelegate* payments_window_delegate)
    : payments_window_delegate_(CHECK_DEREF(payments_window_delegate)) {
  java_payments_window_bridge_ = Java_PaymentsWindowBridge_Constructor(
      base::android::AttachCurrentThread(), reinterpret_cast<int64_t>(this));
}

PaymentsWindowBridge::~PaymentsWindowBridge() {
  if (java_payments_window_bridge_) {
    Java_PaymentsWindowBridge_onNativeDestroyed(
        base::android::AttachCurrentThread(), java_payments_window_bridge_);
  }
}

void PaymentsWindowBridge::OpenEphemeralTab(
    const GURL& url,
    const std::u16string& title,
    content::WebContents& merchant_web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PaymentsWindowBridge_openEphemeralTab(
      env, java_payments_window_bridge_, url, title, &merchant_web_contents);
}

void PaymentsWindowBridge::CloseEphemeralTab() {
  Java_PaymentsWindowBridge_closeEphemeralTab(
      base::android::AttachCurrentThread(), java_payments_window_bridge_);
}

void PaymentsWindowBridge::OnNavigationFinished(const GURL& clicked_url) {
  payments_window_delegate_->OnDidFinishNavigationForBnpl(clicked_url);
}

void PaymentsWindowBridge::OnWebContentsObservationStarted(
    content::WebContents* web_contents) {
  if (web_contents) {
    payments_window_delegate_->OnWebContentsObservationStarted(*web_contents);
  }
}

void PaymentsWindowBridge::OnWebContentsDestroyed() {
  payments_window_delegate_->WebContentsDestroyed();
}

void PaymentsWindowBridge::OnUserDeniedTabOpening() {
  payments_window_delegate_->OnUserDeniedTabOpening();
}

}  // namespace autofill::payments

DEFINE_JNI(PaymentsWindowBridge)
