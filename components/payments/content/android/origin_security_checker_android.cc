// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/OriginSecurityChecker_jni.h"

namespace payments {
namespace {

using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::JavaParamRef;

}  // namespace

// static
jboolean JNI_OriginSecurityChecker_IsOriginSecure(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_url) {
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  return url.is_valid() && network::IsUrlPotentiallyTrustworthy(url);
}

// static
jboolean JNI_OriginSecurityChecker_IsSchemeCryptographic(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_url) {
  GURL url = url::GURLAndroid::ToNativeGURL(env, j_url);
  return url.is_valid() && url.SchemeIsCryptographic();
}

}  // namespace payments
