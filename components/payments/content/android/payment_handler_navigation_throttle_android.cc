// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/android/jni_android.h"
#include "components/payments/content/payment_handler_navigation_throttle.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/PaymentHandlerNavigationThrottle_jni.h"

namespace payments {
namespace android {
// static
void JNI_PaymentHandlerNavigationThrottle_MarkPaymentHandlerWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return;
  PaymentHandlerNavigationThrottle::MarkPaymentHandlerWebContents(web_contents);
}
}  // namespace android
}  // namespace payments
