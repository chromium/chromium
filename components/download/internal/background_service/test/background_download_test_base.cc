// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/test/background_download_test_base.h"

#include "base/functional/callback_helpers.h"

using net::test_server::HttpMethod;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace download {
namespace test {

const char* BackgroundDownloadTestBase::kDefaultResponseContent = "1234";

BackgroundDownloadTestBase::BackgroundDownloadTestBase() = default;
BackgroundDownloadTestBase::~BackgroundDownloadTestBase() = default;

void BackgroundDownloadTestBase::SetUp() {
  ASSERT_TRUE(dir_.CreateUniqueTempDir());
  server_.RegisterRequestHandler(base::BindRepeating(
      &BackgroundDownloadTestBase::DefaultResponse, base::Unretained(this)));
  server_.RegisterRequestHandler(base::BindRepeating(
      &BackgroundDownloadTestBase::NotFoundResponse, base::Unretained(this)));
  server_handle_ = server_.StartAndReturnHandle();
}

std::unique_ptr<HttpResponse> BackgroundDownloadTestBase::DefaultResponse(
    const HttpRequest& request) {
  if (request.relative_url.find("/test") == std::string::npos)
    return nullptr;

  request_sent_ = std::make_unique<HttpRequest>(request);
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(kDefaultResponseContent);
  response->set_content_type("text/plain");
  return response;
}

std::unique_ptr<HttpResponse> BackgroundDownloadTestBase::NotFoundResponse(
    const HttpRequest& request) {
  if (request.relative_url.find("/notfound") == std::string::npos)
    return nullptr;

  request_sent_ = std::make_unique<HttpRequest>(request);
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_NOT_FOUND);
  response->set_content(kDefaultResponseContent);
  response->set_content_type("text/plain");
  return response;
}

}  // namespace test
}  // namespace download
