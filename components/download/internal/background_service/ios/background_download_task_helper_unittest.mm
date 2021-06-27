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

const char kDefaultResponseContent[] = "1234";

namespace download {

std::unique_ptr<HttpResponse> DefaultResponse(const HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HTTP_OK);
  response->set_content(kDefaultResponseContent);
  response->set_content_type("text/plain");
  return response;
}

class BackgroundDownloadTaskHelperTest : public PlatformTest {
 protected:
  BackgroundDownloadTaskHelperTest() {}
  ~BackgroundDownloadTaskHelperTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    server_.RegisterRequestHandler(base::BindRepeating(&DefaultResponse));
    server_handle_ = server_.StartAndReturnHandle();
    helper_ = BackgroundDownloadTaskHelper::Create(dir_.GetPath());
  }

  void Download(const std::string& relative_url) {
    DownloadParams params;
    params.request_params.url = server_.GetURL(relative_url);
    base::RunLoop loop;
    helper_->StartDownload(
        params,
        base::BindLambdaForTesting([&](bool, const base::FilePath& file_path) {
          std::string content;
          ASSERT_TRUE(base::ReadFileToString(file_path, &content));
          EXPECT_EQ(kDefaultResponseContent, content);
          loop.Quit();
        }));
    loop.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  net::EmbeddedTestServer server_;
  net::test_server::EmbeddedTestServerHandle server_handle_;
  base::ScopedTempDir dir_;
  std::unique_ptr<BackgroundDownloadTaskHelper> helper_;
};

// Verifies download can be finished.
TEST_F(BackgroundDownloadTaskHelperTest, DownloadComplete) {
  Download("/test");
}

}  // namespace download
