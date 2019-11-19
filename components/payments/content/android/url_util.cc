// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/url_util.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/payments/content/android/jni_headers/UrlUtil_jni.h"
#include "url/gurl.h"

namespace payments {
namespace android {

// static
jboolean JNI_UrlUtil_IsOriginAllowedToUseWebPaymentApis(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jurl) {
  return UrlUtil::IsOriginAllowedToUseWebPaymentApis(
      GURL(base::android::ConvertJavaStringToUTF8(env, jurl)));
}

// static
jboolean JNI_UrlUtil_IsLocalDevelopmentUrl(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jurl) {
  return UrlUtil::IsLocalDevelopmentUrl(
      GURL(base::android::ConvertJavaStringToUTF8(env, jurl)));
}

}  // namespace android
}  // namespace payments
