// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/payments/content/ssl_validity_checker.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/SslValidityChecker_jni.h"

namespace payments {

// static
base::android::ScopedJavaLocalRef<jstring>
JNI_SslValidityChecker_GetInvalidSslCertificateErrorMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  // SslValidityChecker checks for null `web_contents` parameter.
  return base::android::ConvertUTF8ToJavaString(
      env,
      SslValidityChecker::GetInvalidSslCertificateErrorMessage(web_contents));
}

// static
jboolean JNI_SslValidityChecker_IsValidPageInPaymentHandlerWindow(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  // SslValidityChecker checks for null `web_contents` parameter.
  return SslValidityChecker::IsValidPageInPaymentHandlerWindow(
      content::WebContents::FromJavaWebContents(jweb_contents));
}

}  // namespace payments
