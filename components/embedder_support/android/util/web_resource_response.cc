// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/util/web_resource_response.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/embedder_support/android/util/input_stream.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "third_party/jni_zero/default_conversions.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/util_jni_headers/WebResourceResponseInfo_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;

namespace embedder_support {

namespace {

// We include null chars to prevent collisions with any valid cookie header
// names and values.
// LINT.IfChange(MultiCookieKeys)
constexpr char kCookieMultiHeaderNameChars[] = "\0Set-Cookie-Multivalue\0";
constexpr char kCookieMultiHeaderValueSeparatorChars[] = "\0";
// LINT.ThenChange(/android_webview/support_library/boundary_interfaces/src/org/chromium/support_lib_boundary/WebViewProviderFactoryBoundaryInterface.java:MultiCookieKeys)

// std::string_literals are banned by the style guide, so this code manually
// constructs the string_view instances with explicit length to ensure they
// contain the \0 characters. Subtracting 1 to remove the compiler-inserted null
// terminator.
constexpr std::string_view kCookieMultiHeaderName(
    kCookieMultiHeaderNameChars,
    sizeof(kCookieMultiHeaderNameChars) - 1);
constexpr std::string_view kCookieMultiHeaderValueSeparator(
    kCookieMultiHeaderValueSeparatorChars,
    sizeof(kCookieMultiHeaderValueSeparatorChars) - 1);

}  // namespace

WebResourceResponse::WebResourceResponse(
    const base::android::JavaRef<jobject>& obj)
    : java_object_(obj) {}

WebResourceResponse::~WebResourceResponse() = default;

bool WebResourceResponse::HasInputStream(JNIEnv* env) const {
  return Java_WebResourceResponseInfo_hasInputStream(env, java_object_);
}

std::unique_ptr<InputStream> WebResourceResponse::GetInputStream(JNIEnv* env) {
  return Java_WebResourceResponseInfo_transferStreamToNative(env, java_object_);
}

bool WebResourceResponse::GetMimeType(JNIEnv* env,
                                      std::string* mime_type) const {
  std::optional<std::string> opt_mime_type =
      Java_WebResourceResponseInfo_getMimeType(env, java_object_);

  if (!opt_mime_type) {
    return false;
  }
  *mime_type = *opt_mime_type;
  return true;
}

bool WebResourceResponse::GetCharset(JNIEnv* env, std::string* charset) const {
  std::optional<std::string> opt_charset =
      Java_WebResourceResponseInfo_getCharset(env, java_object_);
  if (!opt_charset) {
    return false;
  }
  *charset = *opt_charset;
  return true;
}

bool WebResourceResponse::GetStatusInfo(JNIEnv* env,
                                        int* status_code,
                                        std::string* reason_phrase) const {
  int status = Java_WebResourceResponseInfo_getStatusCode(env, java_object_);
  std::optional<std::string> opt_reason_phrase =
      Java_WebResourceResponseInfo_getReasonPhrase(env, java_object_);
  if (status < 100 || status >= 600 || !opt_reason_phrase) {
    return false;
  }
  *status_code = status;
  *reason_phrase = *opt_reason_phrase;
  return true;
}

bool WebResourceResponse::GetResponseHeaders(
    JNIEnv* env,
    net::HttpResponseHeaders* headers) const {
  base::flat_map<std::string, std::string> response_headers =
      Java_WebResourceResponseInfo_getResponseHeaders(env, java_object_);

  bool used_multi_cookie_header = false;
  bool did_modify_headers = false;
  for (const auto& [name, value] : response_headers) {
    if (name == kCookieMultiHeaderName) {
      used_multi_cookie_header = true;
      // The Set-Cookie header is special, in that it cannot be easily joined
      // with comma separation. Instead, we allow the client to provide multiple
      // values separated by the null character, which is not a legal value in
      // header values, so we can then split it here.
      //
      // We do this because the API uses a simple string->string map to supply
      // response header values, so we cannot cleanly send more than one value
      // for each key.
      std::vector<std::string_view> cookies = base::SplitStringPiece(
          value, kCookieMultiHeaderValueSeparator, base::KEEP_WHITESPACE,
          base::SPLIT_WANT_NONEMPTY);
      for (const auto& cookie : cookies) {
        headers->AddCookie(cookie);
      }
      did_modify_headers |= !cookies.empty();
    } else {
      headers->AddHeader(name, value);
      did_modify_headers = true;
    }
  }

  base::UmaHistogramBoolean(
      "Android.WebView.ShouldInterceptRequest.DidIncludeMultiCookieHeader",
      used_multi_cookie_header);
  return did_modify_headers;
}

}  // namespace embedder_support
