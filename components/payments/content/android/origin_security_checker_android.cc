// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/payments/content/android/jni_headers/OriginSecurityChecker_jni.h"
#include "content/public/common/origin_util.h"
#include "url/gurl.h"

namespace payments {
namespace {

using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::JavaParamRef;

}  // namespace

// static
jboolean JNI_OriginSecurityChecker_IsOriginSecure(
    JNIEnv* env,
    const JavaParamRef<jstring>& jurl) {
  GURL url(ConvertJavaStringToUTF8(env, jurl));
  return url.is_valid() && content::IsOriginSecure(url);
}

// static
jboolean JNI_OriginSecurityChecker_IsSchemeCryptographic(
    JNIEnv* env,
    const JavaParamRef<jstring>& jurl) {
  GURL url(ConvertJavaStringToUTF8(env, jurl));
  return url.is_valid() && url.SchemeIsCryptographic();
}

}  // namespace payments
