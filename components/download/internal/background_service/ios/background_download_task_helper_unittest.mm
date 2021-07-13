// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/ios/background_download_task_helper.h"

#include <memory>

#import "base/callback_helpers.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/run_loop.h"
#import "base/test/bind.h"
#import "base/test/gmock_callback_support.h"
#import "base/test/task_environment.h"
#import "components/download/public/background_service/download_params.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using net::test_server::HttpMethod;

const char kDefaultResponseContent[] = "1234";
const char kHeaderValue[] = "abcd1234";
const char kGuid[] = "kale consumer";

namespace download {

class BackgroundDownloadTaskHelperTest : public PlatformTest {
 protected:
  BackgroundDownloadTaskHelperTest() {}
  ~BackgroundDownloadTaskHelperTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    server_.RegisterRequestHandler(
        base::BindRepeating(&BackgroundDownloadTaskHelperTest::DefaultResponse,
                            base::Unretained(this)));
    server_handle_ = server_.StartAndReturnHandle();
    helper_ = BackgroundDownloadTaskHelper::Create(dir_.GetPath());
  }

  void Download(const std::string& relative_url) {
    DownloadParams params;
    params.request_params.url = server_.GetURL(relative_url);
    params.request_params.method = "POST";
    params.request_params.request_headers.SetHeader(
        net::HttpRequestHeaders::kIfMatch, kHeaderValue);
    base::RunLoop loop;
    helper_->StartDownload(
        kGuid, params.request_params, params.scheduling_params,
        base::BindLambdaForTesting([&](bool, const base::FilePath& file_path) {
          std::string content;
          ASSERT_TRUE(base::ReadFileToString(file_path, &content));
          EXPECT_EQ(kDefaultResponseContent, content);
          loop.Quit();
        }),
        base::DoNothing());
    loop.Run();
    DCHECK(request_sent_);
    auto it = request_sent_->headers.find(net::HttpRequestHeaders::kIfMatch);
    EXPECT_EQ(kHeaderValue, it->second);
    EXPECT_EQ(HttpMethod::METHOD_POST, request_sent_->method);
  }

  const HttpRequest* request_sent() const { return request_sent_.get(); }
  const base::ScopedTempDir& dir() const { return dir_; }

 private:
  std::unique_ptr<HttpResponse> DefaultResponse(const HttpRequest& request) {
    request_sent_.reset(new HttpRequest(request));
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content(kDefaultResponseContent);
    response->set_content_type("text/plain");
    return response;
  }

  base::test::TaskEnvironment task_environment_;
  net::EmbeddedTestServer server_;
  net::test_server::EmbeddedTestServerHandle server_handle_;
  std::unique_ptr<HttpRequest> request_sent_;
  base::ScopedTempDir dir_;
  std::unique_ptr<BackgroundDownloadTaskHelper> helper_;
};

// Verifies download can be finished.
TEST_F(BackgroundDownloadTaskHelperTest, DownloadComplete) {
  Download("/test");
  EXPECT_TRUE(base::PathExists(dir().GetPath().AppendASCII(kGuid)));
}

}  // namespace download
