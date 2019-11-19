// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/url_formatter/android/jni_headers/UrlFormatter_jni.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_fixer.h"
#include "components/url_formatter/url_formatter.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace {

GURL JNI_UrlFormatter_ConvertJavaStringToGURL(JNIEnv* env, jstring url) {
  return url ? GURL(base::android::ConvertJavaStringToUTF8(env, url)) : GURL();
}

}  // namespace

namespace url_formatter {

namespace android {

static ScopedJavaLocalRef<jstring> JNI_UrlFormatter_FixupUrl(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  DCHECK(url);
  GURL fixed_url = url_formatter::FixupURL(
      base::android::ConvertJavaStringToUTF8(env, url), std::string());

  return fixed_url.is_valid()
             ? base::android::ConvertUTF8ToJavaString(env, fixed_url.spec())
             : ScopedJavaLocalRef<jstring>();
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
               net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
}

static ScopedJavaLocalRef<jstring>
JNI_UrlFormatter_FormatUrlForDisplayOmitHTTPScheme(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrl(
               JNI_UrlFormatter_ConvertJavaStringToGURL(env, url),
               url_formatter::kFormatUrlOmitDefaults, net::UnescapeRule::SPACES,
               nullptr, nullptr, nullptr));
}

static ScopedJavaLocalRef<jstring> JNI_UrlFormatter_FormatUrlForCopy(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrl(
               JNI_UrlFormatter_ConvertJavaStringToGURL(env, url),
               url_formatter::kFormatUrlOmitNothing, net::UnescapeRule::NORMAL,
               nullptr, nullptr, nullptr));
}

static ScopedJavaLocalRef<jstring> JNI_UrlFormatter_FormatUrlForSecurityDisplay(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(
               JNI_UrlFormatter_ConvertJavaStringToGURL(env, url)));
}

static ScopedJavaLocalRef<jstring>
JNI_UrlFormatter_FormatUrlForSecurityDisplayOmitScheme(
    JNIEnv* env,
    const JavaParamRef<jstring>& url) {
  return base::android::ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(
               JNI_UrlFormatter_ConvertJavaStringToGURL(env, url),
               url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS));
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
               net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
}

}  // namespace android

}  // namespace url_formatter
