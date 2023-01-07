// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_HTTP_HTTP_AGENT_IMPL_H_
#define CHROME_CHROME_CLEANER_HTTP_HTTP_AGENT_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/strings/string_piece.h"
#include "chrome/chrome_cleaner/http/http_agent.h"

namespace chrome_cleaner {

// Implements HttpAgent using WinHttp. Respects the user proxy settings if any.
class HttpAgentImpl : public HttpAgent {
 public:
  // Constructs an HttpAgentImpl.
  // @param product_name The product name to include in the User-Agent header.
  // @param product_version The product version to include in the User-Agent
  //     header.
  HttpAgentImpl(base::WStringPiece product_name,
                base::WStringPiece product_version);

  HttpAgentImpl(const HttpAgentImpl&) = delete;
  HttpAgentImpl& operator=(const HttpAgentImpl&) = delete;

  ~HttpAgentImpl() override;

  // HttpAgent implementation
  std::unique_ptr<HttpResponse> Post(
      const std::wstring& host,
      uint16_t port,
      const std::wstring& path,
      bool secure,
      const std::wstring& extra_headers,
      const std::string& body,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

  std::unique_ptr<HttpResponse> Get(
      const std::wstring& host,
      uint16_t port,
      const std::wstring& path,
      bool secure,
      const std::wstring& extra_headers,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override;

 private:
  std::wstring user_agent_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_HTTP_HTTP_AGENT_IMPL_H_
