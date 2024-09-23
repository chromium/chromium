// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_string.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "net/base/url_util.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/dom_distiller/core/android/jni_headers/DomDistillerUrlUtils_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace dom_distiller {

namespace url_utils {

namespace android {

ScopedJavaLocalRef<jstring> JNI_DomDistillerUrlUtils_GetDistillerViewUrlFromUrl(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_scheme,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_title) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  if (!url.is_valid()) {
    return ScopedJavaLocalRef<jstring>();
  }
  std::string scheme(base::android::ConvertJavaStringToUTF8(env, j_scheme));
  std::string title(base::android::ConvertJavaStringToUTF8(env, j_title));
  GURL view_url =
      dom_distiller::url_utils::GetDistillerViewUrlFromUrl(scheme, url, title);
  if (!view_url.is_valid()) {
    return ScopedJavaLocalRef<jstring>();
  }
  return base::android::ConvertUTF8ToJavaString(env, view_url.spec());
}

ScopedJavaLocalRef<jobject>
JNI_DomDistillerUrlUtils_GetOriginalUrlFromDistillerUrl(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_url) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  if (!url.is_valid())
    return url::GURLAndroid::EmptyGURL(env);

  GURL original_url =
      dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(url);
  if (!original_url.is_valid())
    return url::GURLAndroid::EmptyGURL(env);

  return url::GURLAndroid::FromNativeGURL(env, original_url);
}

jboolean JNI_DomDistillerUrlUtils_IsDistilledPage(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_url) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  return dom_distiller::url_utils::IsDistilledPage(url);
}

ScopedJavaLocalRef<jstring> JNI_DomDistillerUrlUtils_GetValueForKeyInUrl(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jstring>& j_key) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  std::string key = base::android::ConvertJavaStringToUTF8(env, j_key);
  return base::android::ConvertUTF8ToJavaString(
      env, dom_distiller::url_utils::GetValueForKeyInUrl(url, key));
}

}  // namespace android

}  // namespace url_utils

}  // namespace dom_distiller
