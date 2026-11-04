// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/server.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "chrome/updater/test/request_matcher.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace updater::test {

using net::test_server::BasicHttpResponse;

namespace {

// Returns a `POST` request for `relative_url` with `content` as its body.
net::test_server::HttpRequest MakeRequest(const std::string& relative_url,
                                          const std::string& content = {}) {
  net::test_server::HttpRequest request;
  request.relative_url = relative_url;
  request.method = net::test_server::METHOD_POST;
  request.content = content;
  request.has_content = !content.empty();
  return request;
}

// Returns a matcher group that matches requests whose relative URL (path and
// query) full-matches `path_regex`.
request::MatcherGroup PathIs(const std::string& path_regex) {
  return {request::GetPathMatcher(path_regex)};
}

}  // namespace

// Tests `ScopedServer` by passing requests to `ScopedServer::HandleRequest` on
// the test thread, so `EXPECT_NONFATAL_FAILURE` catches the failures it
// reports.
class ScopedServerTest : public ::testing::Test {
 protected:
  // Returns `server_`'s response to `request`.
  std::unique_ptr<BasicHttpResponse> Handle(
      const net::test_server::HttpRequest& request) {
    // `HandleRequest` returns only `BasicHttpResponse` or its subclasses.
    return base::WrapUnique(static_cast<BasicHttpResponse*>(
        server_.HandleRequest(request).release()));
  }

  // Fails the test on destruction if any expectation is unmet.
  ScopedServer server_;
};

TEST_F(ScopedServerTest, ExpectOnceServesResponse) {
  server_.ExpectOnce(PathIs("/update"), "response", net::HTTP_ACCEPTED);

  std::unique_ptr<BasicHttpResponse> response = Handle(MakeRequest("/update"));
  EXPECT_EQ(response->code(), net::HTTP_ACCEPTED);
  EXPECT_EQ(response->content(), "response");
}

TEST_F(ScopedServerTest, ExpectOnceMatchesInOrder) {
  server_.ExpectOnce(PathIs("/first"), "1");
  server_.ExpectOnce(PathIs("/second"), "2");

  std::unique_ptr<BasicHttpResponse> response;
  EXPECT_NONFATAL_FAILURE(response = Handle(MakeRequest("/second")),
                          "Unmatched");
  EXPECT_EQ(response->code(), net::HTTP_INTERNAL_SERVER_ERROR);

  // The mismatched request does not consume an expectation.
  EXPECT_EQ(Handle(MakeRequest("/first"))->content(), "1");
  EXPECT_EQ(Handle(MakeRequest("/second"))->content(), "2");
}

TEST_F(ScopedServerTest, AllMatchersInGroupMustMatch) {
  server_.ExpectOnce({request::GetPathMatcher("/update.*"),
                      request::GetPathMatcher(R"(/update\?x=1)")},
                     "response");

  std::unique_ptr<BasicHttpResponse> response;
  EXPECT_NONFATAL_FAILURE(response = Handle(MakeRequest("/update")),
                          "Unmatched");
  EXPECT_EQ(response->code(), net::HTTP_INTERNAL_SERVER_ERROR);

  EXPECT_EQ(Handle(MakeRequest("/update?x=1"))->content(), "response");
}

TEST_F(ScopedServerTest, UnexpectedRequestFails) {
  std::unique_ptr<BasicHttpResponse> response;
  EXPECT_NONFATAL_FAILURE(response = Handle(MakeRequest("/update")),
                          "Unexpected");
  EXPECT_EQ(response->code(), net::HTTP_INTERNAL_SERVER_ERROR);
}

TEST_F(ScopedServerTest, ResponseBodyProviderReceivesProtocolVersion) {
  const auto provider = base::BindRepeating(
      [](bool is_v4) -> std::string { return is_v4 ? "v4" : "v3"; });
  server_.ExpectOnce(PathIs("/update"), provider);
  server_.ExpectOnce(PathIs("/update"), provider, net::HTTP_ACCEPTED);

  std::unique_ptr<BasicHttpResponse> response =
      Handle(MakeRequest("/update", R"({"request": {"protocol": "4.0"}})"));
  EXPECT_EQ(response->code(), net::HTTP_OK);
  EXPECT_EQ(response->content(), "v4");

  response =
      Handle(MakeRequest("/update", R"({"request": {"protocol": "3.1"}})"));
  EXPECT_EQ(response->code(), net::HTTP_ACCEPTED);
  EXPECT_EQ(response->content(), "v3");
}

TEST_F(ScopedServerTest, DestructorReportsUnmetExpectation) {
  EXPECT_NONFATAL_FAILURE(
      {
        ScopedServer server;
        server.ExpectOnce(PathIs("/update"), "response");
      },
      "Unmet expectation");
}

TEST_F(ScopedServerTest, GzipResponse) {
  server_.set_gzip_response(true);
  server_.ExpectOnce(PathIs("/update"), "response");

  // A request that does not accept gzip fails without consuming the
  // expectation.
  std::unique_ptr<BasicHttpResponse> response;
  EXPECT_NONFATAL_FAILURE(response = Handle(MakeRequest("/update")),
                          "Accept-Encoding");
  EXPECT_EQ(response->code(), net::HTTP_INTERNAL_SERVER_ERROR);

  net::test_server::HttpRequest request = MakeRequest("/update");
  request.headers["Accept-Encoding"] = "gzip, deflate";
  response = Handle(request);
  EXPECT_EQ(response->code(), net::HTTP_OK);
  EXPECT_THAT(response->BuildHeaders(),
              testing::Contains(testing::Pair("Content-Encoding", "gzip")));
  std::string uncompressed;
  ASSERT_TRUE(compression::GzipUncompress(response->content(), &uncompressed));
  EXPECT_EQ(uncompressed, "response");
}

TEST_F(ScopedServerTest, DeviceManagementContentType) {
  server_.ExpectOnce(PathIs("/dmapi.*"), "response");
  server_.ExpectOnce(PathIs("/update"), "response");

  EXPECT_EQ(Handle(MakeRequest("/dmapi?request=policy"))->content_type(),
            "application/x-protobuf");
  EXPECT_EQ(Handle(MakeRequest("/update"))->content_type(), "");
}

TEST_F(ScopedServerTest, DownloadDelay) {
  server_.set_download_delay(base::Seconds(1));
  server_.ExpectOnce(PathIs("/download/.*"), "payload", net::HTTP_ACCEPTED);

  std::unique_ptr<BasicHttpResponse> response =
      Handle(MakeRequest("/download/file"));
  EXPECT_EQ(response->code(), net::HTTP_ACCEPTED);
  EXPECT_EQ(response->content(), "payload");
}

}  // namespace updater::test
