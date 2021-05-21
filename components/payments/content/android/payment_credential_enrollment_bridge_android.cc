// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_credential_enrollment_bridge_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "build/build_config.h"
#include "components/payments/content/android/jni_headers/PaymentCredentialEnrollmentBridgeAndroid_jni.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/android/java_bitmap.h"

namespace payments {

using base::android::ConvertUTF16ToJavaString;
using base::android::ScopedJavaLocalRef;

// static
std::unique_ptr<PaymentCredentialEnrollmentBridge>
PaymentCredentialEnrollmentBridge::Create() {
  return std::make_unique<PaymentCredentialEnrollmentBridgeAndroid>();
}

void PaymentCredentialEnrollmentBridgeAndroid::ShowDialog(
    content::WebContents* web_contents,
    std::unique_ptr<SkBitmap> instrument_icon,
    const std::u16string& instrument_name,
    ResponseCallback response_callback) {
  if (!web_contents || !web_contents->GetBrowserContext())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!env)
    return;
  ScopedJavaLocalRef<jobject> icon = gfx::ConvertToJavaBitmap(*instrument_icon);
  if (!icon)
    return;
  auto* response_callback_ptr =
      new ResponseCallback(std::move(response_callback));
  Java_PaymentCredentialEnrollmentBridgeAndroid_showDialog(
      env, web_contents->GetJavaWebContents(),
      ConvertUTF16ToJavaString(env, instrument_name),
      web_contents->GetBrowserContext()->IsOffTheRecord(), icon,
      reinterpret_cast<jlong>(response_callback_ptr));
}

void PaymentCredentialEnrollmentBridgeAndroid::ShowProcessingSpinner() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!env)
    return;
  Java_PaymentCredentialEnrollmentBridgeAndroid_showProcessingSpinner(env);
}

void PaymentCredentialEnrollmentBridgeAndroid::CloseDialog() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!env)
    return;
  Java_PaymentCredentialEnrollmentBridgeAndroid_closeDialog(env);
}

}  // namespace payments

static void JNI_PaymentCredentialEnrollmentBridgeAndroid_OnResponse(
    JNIEnv* env,
    jlong callback_ptr,
    jboolean acceptded) {
  auto* callback = reinterpret_cast<
      payments::PaymentCredentialEnrollmentBridge::ResponseCallback*>(
      callback_ptr);
  if (!callback)
    return;
  std::move(*callback).Run(acceptded);
}
