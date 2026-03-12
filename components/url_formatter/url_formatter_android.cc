// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_formatter/url_formatter.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_fixer.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/url_formatter/android/jni_headers/UrlFormatter_jni.h"

namespace url_formatter {

namespace android {

static GURL JNI_UrlFormatter_FixupUrl(const std::string& url) {
  return url_formatter::FixupURL(url);
}

static std::u16string JNI_UrlFormatter_FormatUrlForDisplayOmitScheme(
    const std::string& url) {
  return url_formatter::FormatUrl(GURL(url),
                                  url_formatter::kFormatUrlOmitDefaults |
                                      url_formatter::kFormatUrlOmitHTTPS,
                                  base::UnescapeRule::SPACES, nullptr, nullptr,
                                  nullptr);
}

static std::u16string JNI_UrlFormatter_FormatUrlForDisplayOmitHTTPScheme(
    const std::string& url) {
  return url_formatter::FormatUrl(
      GURL(url), url_formatter::kFormatUrlOmitDefaults,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
}

static std::u16string JNI_UrlFormatter_FormatUrlForDisplayOmitUsernamePassword(
    const std::string& url) {
  return url_formatter::FormatUrl(
      GURL(url),
      url_formatter::kFormatUrlOmitUsernamePassword |
          kFormatUrlOmitTrailingSlashOnBareHostname,
      base::UnescapeRule::NONE, nullptr, nullptr, nullptr);
}

static std::u16string JNI_UrlFormatter_FormatUrlForCopy(
    const std::string& url) {
  return url_formatter::FormatUrl(
      GURL(url), url_formatter::kFormatUrlOmitNothing,
      base::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
}

static std::u16string JNI_UrlFormatter_FormatStringUrlForSecurityDisplay(
    const std::string& url,
    int32_t scheme_display) {
  return url_formatter::FormatUrlForSecurityDisplay(
      GURL(url), static_cast<SchemeDisplay>(scheme_display));
}

static std::u16string JNI_UrlFormatter_FormatUrlForSecurityDisplay(
    const GURL& gurl,
    int32_t scheme_display) {
  return url_formatter::FormatUrlForSecurityDisplay(
      gurl, static_cast<SchemeDisplay>(scheme_display));
}

static std::u16string JNI_UrlFormatter_FormatOriginForSecurityDisplay(
    const url::Origin& origin,
    int32_t scheme_display) {
  return url_formatter::FormatOriginForSecurityDisplay(
      origin, static_cast<SchemeDisplay>(scheme_display));
}

static std::u16string
JNI_UrlFormatter_FormatUrlForDisplayOmitSchemeOmitTrivialSubdomains(
    const std::string& url) {
  return url_formatter::FormatUrl(
      GURL(url),
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains,
      base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
}

static std::u16string
JNI_UrlFormatter_FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
    const GURL& gurl) {
  return url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
      gurl);
}

}  // namespace android

}  // namespace url_formatter

DEFINE_JNI(UrlFormatter)
