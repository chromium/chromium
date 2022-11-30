// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/util/response_delegate_impl.h"

#include "base/strings/string_number_conversions.h"
#include "components/embedder_support/android/util/input_stream.h"
#include "components/embedder_support/android/util/web_resource_response.h"

namespace embedder_support {

ResponseDelegateImpl::ResponseDelegateImpl(
    std::unique_ptr<WebResourceResponse> response)
    : response_(std::move(response)) {}

ResponseDelegateImpl::~ResponseDelegateImpl() = default;

std::unique_ptr<InputStream> ResponseDelegateImpl::OpenInputStream(
    JNIEnv* env) {
  return response_->GetInputStream(env);
}

bool ResponseDelegateImpl::OnInputStreamOpenFailed() {
  return true;
}

bool ResponseDelegateImpl::GetMimeType(JNIEnv* env,
                                       const GURL& url,
                                       InputStream* stream,
                                       std::string* mime_type) {
  return response_->GetMimeType(env, mime_type);
}

void ResponseDelegateImpl::GetCharset(JNIEnv* env,
                                      const GURL& url,
                                      InputStream* stream,
                                      std::string* charset) {
  response_->GetCharset(env, charset);
}

void ResponseDelegateImpl::AppendResponseHeaders(
    JNIEnv* env,
    net::HttpResponseHeaders* headers) {
  int status_code;
  std::string reason_phrase;
  if (response_->GetStatusInfo(env, &status_code, &reason_phrase)) {
    std::string status_line("HTTP/1.1 ");
    status_line.append(base::NumberToString(status_code));
    status_line.append(" ");
    status_line.append(reason_phrase);
    headers->ReplaceStatusLine(status_line);
  }
  response_->GetResponseHeaders(env, headers);
}

}  // namespace embedder_support
