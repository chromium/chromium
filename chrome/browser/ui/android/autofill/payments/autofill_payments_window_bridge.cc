// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/payments/autofill_payments_window_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check_deref.h"
#include "chrome/browser/ui/android/autofill/internal/jni_headers/PaymentsWindowBridge_jni.h"
#include "chrome/browser/ui/android/autofill/payments/autofill_payments_window_delegate.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

namespace autofill::payments {

using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;

AutofillPaymentsWindowBridge::AutofillPaymentsWindowBridge(
    AutofillPaymentsWindowDelegate* autofill_payments_window_delegate)
    : autofill_payments_window_delegate_(
          CHECK_DEREF(autofill_payments_window_delegate)) {
  java_autofill_payments_window_bridge_ = Java_PaymentsWindowBridge_Constructor(
      base::android::AttachCurrentThread(), reinterpret_cast<jlong>(this));
}

AutofillPaymentsWindowBridge::~AutofillPaymentsWindowBridge() = default;

void AutofillPaymentsWindowBridge::OpenEphemeralTab(
    const GURL& url,
    const std::u16string& title,
    content::WebContents& merchant_web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PaymentsWindowBridge_openEphemeralTab(
      env, java_autofill_payments_window_bridge_,
      url::GURLAndroid::FromNativeGURL(env, url),
      ConvertUTF16ToJavaString(env, title),
      merchant_web_contents.GetJavaWebContents());
}

void AutofillPaymentsWindowBridge::CloseEphemeralTab() {
  Java_PaymentsWindowBridge_closeEphemeralTab(
      base::android::AttachCurrentThread(),
      java_autofill_payments_window_bridge_);
}

void AutofillPaymentsWindowBridge::OnNavigationFinished(
    JNIEnv* env,
    const JavaParamRef<jobject>& clicked_url_object) {
  autofill_payments_window_delegate_->OnDidFinishNavigationForBnpl(
      url::GURLAndroid::ToNativeGURL(env, clicked_url_object));
}

void AutofillPaymentsWindowBridge::OnWebContentsDestroyed(JNIEnv* env) {
  autofill_payments_window_delegate_->WebContentsDestroyed();
}

}  // namespace autofill::payments
