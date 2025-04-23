// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/util/web_resource_response.h"

#include <memory>
#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/flat_map.h"
#include "components/embedder_support/android/util/input_stream.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/util_jni_headers/WebResourceResponseInfo_jni.h"

using base::android::AppendJavaStringArrayToStringVector;
using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaLocalRef;

namespace embedder_support {

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
  std::optional<base::flat_map<std::string, std::string>> opt_headers =
      Java_WebResourceResponseInfo_getResponseHeaders(env, java_object_);
  if (!opt_headers) {
    return false;
  }
  for (const auto& entry : *opt_headers) {
    headers->AddHeader(entry.first, entry.second);
  }
  return true;
}

}  // namespace embedder_support
