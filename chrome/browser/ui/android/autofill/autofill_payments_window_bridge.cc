// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_payments_window_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/ui/android/autofill/internal/jni_headers/PaymentsWindowBridge_jni.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::ConvertUTF16ToJavaString;

namespace autofill {

AutofillPaymentsWindowBridge::AutofillPaymentsWindowBridge(
    content::WebContents& web_contents) {
  java_autofill_payments_window_bridge_ = Java_PaymentsWindowBridge_Constructor(
      base::android::AttachCurrentThread(), web_contents.GetJavaWebContents());
}

AutofillPaymentsWindowBridge::~AutofillPaymentsWindowBridge() = default;

void AutofillPaymentsWindowBridge::OpenEphemeralTab(
    const GURL& url,
    const std::u16string& title) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PaymentsWindowBridge_openEphemeralTab(
      env, java_autofill_payments_window_bridge_,
      url::GURLAndroid::FromNativeGURL(env, url),
      ConvertUTF16ToJavaString(env, title));
}

void AutofillPaymentsWindowBridge::CloseEphemeralTab() {
  Java_PaymentsWindowBridge_closeEphemeralTab(
      base::android::AttachCurrentThread(),
      java_autofill_payments_window_bridge_);
}
}  // namespace autofill
