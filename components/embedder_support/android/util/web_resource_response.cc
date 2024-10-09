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
    : java_object_(obj), input_stream_transferred_(false) {}

WebResourceResponse::~WebResourceResponse() = default;

bool WebResourceResponse::HasInputStream(JNIEnv* env) const {
  ScopedJavaLocalRef<jobject> jstream =
      Java_WebResourceResponseInfo_getData(env, java_object_);
  return !!jstream;
}

std::unique_ptr<InputStream> WebResourceResponse::GetInputStream(JNIEnv* env) {
  // Only allow to call GetInputStream once per object, because this method
  // transfers ownership of the stream and once the unique_ptr<InputStream>
  // is deleted it also closes the original java input stream. This
  // side-effect can result in unexpected behavior, e.g. trying to read
  // from a closed stream.
  DCHECK(!input_stream_transferred_);

  if (input_stream_transferred_)
    return nullptr;

  input_stream_transferred_ = true;
  ScopedJavaLocalRef<jobject> jstream =
      Java_WebResourceResponseInfo_getData(env, java_object_);
  if (!jstream)
    return nullptr;
  return std::make_unique<InputStream>(jstream);
}

bool WebResourceResponse::GetMimeType(JNIEnv* env,
                                      std::string* mime_type) const {
  ScopedJavaLocalRef<jstring> jstring_mime_type =
      Java_WebResourceResponseInfo_getMimeType(env, java_object_);
  if (!jstring_mime_type)
    return false;
  *mime_type = ConvertJavaStringToUTF8(jstring_mime_type);
  return true;
}

bool WebResourceResponse::GetCharset(JNIEnv* env, std::string* charset) const {
  ScopedJavaLocalRef<jstring> jstring_charset =
      Java_WebResourceResponseInfo_getCharset(env, java_object_);
  if (!jstring_charset)
    return false;
  *charset = ConvertJavaStringToUTF8(jstring_charset);
  return true;
}

bool WebResourceResponse::GetStatusInfo(JNIEnv* env,
                                        int* status_code,
                                        std::string* reason_phrase) const {
  int status = Java_WebResourceResponseInfo_getStatusCode(env, java_object_);
  ScopedJavaLocalRef<jstring> jstring_reason_phrase =
      Java_WebResourceResponseInfo_getReasonPhrase(env, java_object_);
  if (status < 100 || status >= 600 || !jstring_reason_phrase)
    return false;
  *status_code = status;
  *reason_phrase = ConvertJavaStringToUTF8(jstring_reason_phrase);
  return true;
}

bool WebResourceResponse::GetResponseHeaders(
    JNIEnv* env,
    net::HttpResponseHeaders* headers) const {
  ScopedJavaLocalRef<jobjectArray> jstringArray_headerNames =
      Java_WebResourceResponseInfo_getResponseHeaderNames(env, java_object_);
  ScopedJavaLocalRef<jobjectArray> jstringArray_headerValues =
      Java_WebResourceResponseInfo_getResponseHeaderValues(env, java_object_);
  if (!jstringArray_headerNames || !jstringArray_headerValues)
    return false;
  std::vector<std::string> header_names;
  std::vector<std::string> header_values;
  AppendJavaStringArrayToStringVector(env, jstringArray_headerNames,
                                      &header_names);
  AppendJavaStringArrayToStringVector(env, jstringArray_headerValues,
                                      &header_values);
  DCHECK_EQ(header_values.size(), header_names.size());
  for (size_t i = 0; i < header_names.size(); ++i) {
    headers->AddHeader(header_names[i], header_values[i]);
  }
  return true;
}

}  // namespace embedder_support
