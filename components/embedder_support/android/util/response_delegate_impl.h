// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_RESPONSE_DELEGATE_IMPL_H_
#define COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_RESPONSE_DELEGATE_IMPL_H_

#include "components/embedder_support/android/util/android_stream_reader_url_loader.h"

namespace embedder_support {
class WebResourceResponse;

// A ResponseDelegate for responses that get data from WebResourceResponse.
class ResponseDelegateImpl
    : public AndroidStreamReaderURLLoader::ResponseDelegate {
 public:
  explicit ResponseDelegateImpl(std::unique_ptr<WebResourceResponse> response);
  ~ResponseDelegateImpl() override;

  // AndroidStreamReaderURLLoader::ResponseDelegate implementation:
  std::unique_ptr<InputStream> OpenInputStream(JNIEnv* env) override;
  bool OnInputStreamOpenFailed() override;
  bool GetMimeType(JNIEnv* env,
                   const GURL& url,
                   InputStream* stream,
                   std::string* mime_type) override;
  void GetCharset(JNIEnv* env,
                  const GURL& url,
                  InputStream* stream,
                  std::string* charset) override;
  void AppendResponseHeaders(JNIEnv* env,
                             net::HttpResponseHeaders* headers) override;

 private:
  std::unique_ptr<WebResourceResponse> response_;
};

}  // namespace embedder_support

#endif  //  COMPONENTS_EMBEDDER_SUPPORT_ANDROID_UTIL_RESPONSE_DELEGATE_IMPL_H_
