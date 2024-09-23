// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/url_util.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/UrlUtil_jni.h"

namespace payments {
namespace android {

// static
jboolean JNI_UrlUtil_IsOriginAllowedToUseWebPaymentApis(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_url) {
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  return UrlUtil::IsOriginAllowedToUseWebPaymentApis(url);
}

// static
jboolean JNI_UrlUtil_IsValidUrlBasedPaymentMethodIdentifier(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_url) {
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  return UrlUtil::IsValidUrlBasedPaymentMethodIdentifier(url);
}

// static
jboolean JNI_UrlUtil_IsLocalDevelopmentUrl(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_url) {
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  return UrlUtil::IsLocalDevelopmentUrl(url);
}

}  // namespace android
}  // namespace payments
