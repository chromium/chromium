// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_WEB_RESOURCE_RESPONSE_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_WEB_RESOURCE_RESPONSE_H_

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"

namespace net {
class HttpResponseHeaders;
}

namespace embedder_support {
class InputStream;

// This class represents the Java-side data that is to be used to complete a
// particular URLRequest.
class WebResourceResponse {
 public:
  // It is expected that |obj| is an instance of the Java-side
  // org.chromium.components.embedder_support.util.WebResourceResponseInfo
  // class.
  explicit WebResourceResponse(const base::android::JavaRef<jobject>& obj);

  WebResourceResponse(const WebResourceResponse&) = delete;
  WebResourceResponse& operator=(const WebResourceResponse&) = delete;

  ~WebResourceResponse();

  bool HasInputStream(JNIEnv* env) const;
  std::unique_ptr<embedder_support::InputStream> GetInputStream(JNIEnv* env);
  bool GetMimeType(JNIEnv* env, std::string* mime_type) const;
  bool GetCharset(JNIEnv* env, std::string* charset) const;
  bool GetStatusInfo(JNIEnv* env,
                     int* status_code,
                     std::string* reason_phrase) const;
  // If true is returned then |headers| contain the headers, if false is
  // returned |headers| were not updated.
  bool GetResponseHeaders(JNIEnv* env, net::HttpResponseHeaders* headers) const;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  bool input_stream_transferred_;
};

}  // namespace embedder_support

#endif  // COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_WEB_RESOURCE_RESPONSE_H_
