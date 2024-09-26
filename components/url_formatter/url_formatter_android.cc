// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_fixer.h"
#include "components/url_formatter/url_formatter.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/url_formatter/android/jni_headers/UrlFormatter_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

GURL JNI_UrlFormatter_ConvertJavaStringToGURL(JNIEnv* env, jstring url) {
  return url ? GURL(base::android::ConvertJavaStringToUTF8(env, url)) : GURL();
}

}  // namespace

namespace url_formatter {

namespace android {

static ScopedJavaLocalRef<jobject> JNI_UrlFormatter_FixupUrl(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  DCHECK(url);
  GURL fixed_url = url_formatter::FixupURL(
      base::android::ConvertJavaStringToUTF8(env, url), std::string());

  return url::GURLAndroid::FromNativeGURL(env, fixed_url);
}

static ScopedJavaLocalRef<jstring>
JNI_UrlFormatter_FormatUrlForDisplayOmitScheme(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrl(
               JNI_UrlFormatter_ConvertJavaStringToGURL(env, url),
               url_formatter::kFormatUrlOmitDefaults |
                   url_formatter::kFormatUrlOmitHTTPS,
               base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
}

static ScopedJavaLocalRef<jstring>
JNI_UrlFormatter_FormatUrlForDisplayOmitHTTPScheme(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrl(
               JNI_UrlFormatter_ConvertJavaStringToGURL(env, url),
               url_formatter::kFormatUrlOmitDefaults,
               base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
}

static ScopedJavaLocalRef<jstring>
JNI_UrlFormatter_FormatUrlForDisplayOmitUsernamePassword(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrl(
               JNI_UrlFormatter_ConvertJavaStringToGURL(env, url),
               url_formatter::kFormatUrlOmitUsernamePassword |
                   kFormatUrlOmitTrailingSlashOnBareHostname,
               base::UnescapeRule::NONE, nullptr, nullptr, nullptr));
}

static ScopedJavaLocalRef<jstring> JNI_UrlFormatter_FormatUrlForCopy(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrl(
               JNI_UrlFormatter_ConvertJavaStringToGURL(env, url),
               url_formatter::kFormatUrlOmitNothing, base::UnescapeRule::NORMAL,
               nullptr, nullptr, nullptr));
}

static ScopedJavaLocalRef<jstring>
JNI_UrlFormatter_FormatStringUrlForSecurityDisplay(
    JNIEnv* env,
    const JavaParamRef<jstring>& url,
    jint scheme_display) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(
               JNI_UrlFormatter_ConvertJavaStringToGURL(env, url),
               static_cast<SchemeDisplay>(scheme_display)));
}

static ScopedJavaLocalRef<jstring> JNI_UrlFormatter_FormatUrlForSecurityDisplay(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_gurl,
    jint scheme_display) {
  DCHECK(j_gurl);
  GURL gurl = url::GURLAndroid::ToNativeGURL(env, j_gurl);
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(
               gurl, static_cast<SchemeDisplay>(scheme_display)));
}

static ScopedJavaLocalRef<jstring>
JNI_UrlFormatter_FormatOriginForSecurityDisplay(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_origin,
    jint scheme_display) {
  DCHECK(j_origin);
  url::Origin origin = url::Origin::FromJavaObject(env, j_origin);
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatOriginForSecurityDisplay(
               origin, static_cast<SchemeDisplay>(scheme_display)));
}

static ScopedJavaLocalRef<jstring>
JNI_UrlFormatter_FormatUrlForDisplayOmitSchemeOmitTrivialSubdomains(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrl(
               JNI_UrlFormatter_ConvertJavaStringToGURL(env, url),
               url_formatter::kFormatUrlOmitDefaults |
                   url_formatter::kFormatUrlOmitHTTPS |
                   url_formatter::kFormatUrlOmitTrivialSubdomains,
               base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
}

static ScopedJavaLocalRef<jstring>
JNI_UrlFormatter_FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_gurl) {
  DCHECK(j_gurl);
  GURL gurl = url::GURLAndroid::ToNativeGURL(env, j_gurl);
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
               gurl));
}

}  // namespace android

}  // namespace url_formatter
