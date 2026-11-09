// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/core/url_utils.h"

#include <string>

#include "base/android/jni_string.h"
#include "components/web_distiller/core/url_constants.h"
#include "net/base/url_util.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/web_distiller/core/android/jni_headers/WebDistillerUrlUtils_jni.h"

using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace web_distiller {

namespace url_utils {

namespace android {

static ScopedJavaLocalRef<jstring>
JNI_WebDistillerUrlUtils_GetDistillerViewUrlFromUrl(
    JNIEnv* env,
    const JavaRef<jstring>& j_scheme,
    const JavaRef<jstring>& j_url,
    const JavaRef<jstring>& j_title) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  if (!url.is_valid()) {
    return ScopedJavaLocalRef<jstring>();
  }
  std::string scheme(base::android::ConvertJavaStringToUTF8(env, j_scheme));
  std::string title(base::android::ConvertJavaStringToUTF8(env, j_title));
  GURL view_url =
      web_distiller::url_utils::GetDistillerViewUrlFromUrl(scheme, url, title);
  if (!view_url.is_valid()) {
    return ScopedJavaLocalRef<jstring>();
  }
  return base::android::ConvertUTF8ToJavaString(env, view_url.spec());
}

static ScopedJavaLocalRef<jobject>
JNI_WebDistillerUrlUtils_GetOriginalUrlFromDistillerUrl(
    JNIEnv* env,
    const JavaRef<jstring>& j_url) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  if (!url.is_valid()) {
    return url::GURLAndroid::EmptyGURL(env);
  }

  GURL original_url =
      web_distiller::url_utils::GetOriginalUrlFromDistillerUrl(url);
  if (!original_url.is_valid()) {
    return url::GURLAndroid::EmptyGURL(env);
  }

  return url::GURLAndroid::FromNativeGURL(env, original_url);
}

static bool JNI_WebDistillerUrlUtils_IsDistilledPage(
    JNIEnv* env,
    const JavaRef<jstring>& j_url) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  return web_distiller::url_utils::IsDistilledPage(url);
}

static ScopedJavaLocalRef<jstring> JNI_WebDistillerUrlUtils_GetValueForKeyInUrl(
    JNIEnv* env,
    const JavaRef<jstring>& j_url,
    const JavaRef<jstring>& j_key) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, j_url));
  std::string key = base::android::ConvertJavaStringToUTF8(env, j_key);
  return base::android::ConvertUTF8ToJavaString(
      env, web_distiller::url_utils::GetValueForKeyInUrl(url, key));
}

}  // namespace android

}  // namespace url_utils

}  // namespace web_distiller

DEFINE_JNI(WebDistillerUrlUtils)
