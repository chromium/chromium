// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/ios/background_download_task_helper.h"

#include <memory>

#import "base/callback_helpers.h"
#import "base/files/file_util.h"
#import "base/files/scoped_temp_dir.h"
#import "base/run_loop.h"
#import "base/sequence_checker.h"
#import "base/test/bind.h"
#import "base/test/gmock_callback_support.h"
#import "components/download/internal/background_service/test/background_download_test_base.h"
#import "components/download/public/background_service/download_params.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/http_request.h"
#import "net/test/embedded_test_server/http_response.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using net::test_server::HttpMethod;

const char kHeaderValue[] = "abcd1234";
const char kGuid[] = "kale consumer";

namespace download {

class BackgroundDownloadTaskHelperTest
    : public test::BackgroundDownloadTestBase {
 protected:
  BackgroundDownloadTaskHelperTest() {}
  ~BackgroundDownloadTaskHelperTest() override = default;

  void SetUp() override {
    BackgroundDownloadTestBase::SetUp();
    helper_ = BackgroundDownloadTaskHelper::Create();
  }

  void Download(
      const std::string& relative_url,
      BackgroundDownloadTaskHelper::CompletionCallback completion_callback) {
    DownloadParams params;
    params.request_params.url = server_.GetURL(relative_url);
    params.request_params.method = "POST";
    params.request_params.request_headers.SetHeader(
        net::HttpRequestHeaders::kIfMatch, kHeaderValue);
    helper_->StartDownload(kGuid, dir_.GetPath().AppendASCII(kGuid),
                           params.request_params, params.scheduling_params,
                           std::move(completion_callback), base::DoNothing());
  }

  BackgroundDownloadTaskHelper* helper() { return helper_.get(); }

 private:
  std::unique_ptr<BackgroundDownloadTaskHelper> helper_;
};

// Verifies download can be finished.
TEST_F(BackgroundDownloadTaskHelperTest, DownloadComplete) {
  base::RunLoop loop;
  Download(
      "/test", base::BindLambdaForTesting([&](bool success,
                                              const base::FilePath& file_path,
                                              int64_t file_size) {
        std::string content;
        EXPECT_TRUE(success);
        ASSERT_TRUE(base::ReadFileToString(file_path, &content));
        EXPECT_EQ(BackgroundDownloadTestBase::kDefaultResponseContent, content);
        EXPECT_EQ(file_size, static_cast<int64_t>(content.size()));
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        loop.Quit();
      }));
  loop.Run();
  EXPECT_TRUE(base::PathExists(dir().GetPath().AppendASCII(kGuid)));
  DCHECK(request_sent());
  auto it = request_sent()->headers.find(net::HttpRequestHeaders::kIfMatch);
  EXPECT_EQ(kHeaderValue, it->second);
  EXPECT_EQ(HttpMethod::METHOD_POST, request_sent()->method);
}

// Verifies non success http code is treated as error.
TEST_F(BackgroundDownloadTaskHelperTest, DownloadErrorNonSuccessHttpCode) {
  base::RunLoop loop;
  Download("/notfound",
           base::BindLambdaForTesting([&](bool success,
                                          const base::FilePath& file_path,
                                          int64_t file_size) {
             EXPECT_FALSE(success);
             DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
             loop.Quit();
           }));
  loop.Run();
  EXPECT_FALSE(base::PathExists(dir().GetPath().AppendASCII(kGuid)));
}

// Verifies data URL should result in failure.
TEST_F(BackgroundDownloadTaskHelperTest, DataURL) {
  base::RunLoop loop;
  DownloadParams params;
  params.request_params.url = GURL("data:text/plain;base64,Q2hyb21pdW0=");
  helper()->StartDownload(
      kGuid, dir_.GetPath().AppendASCII(kGuid), params.request_params,
      params.scheduling_params,
      base::BindLambdaForTesting([&](bool success,
                                     const base::FilePath& file_path,
                                     int64_t file_size) {
        EXPECT_FALSE(success);
        DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
        loop.Quit();
      }),
      base::DoNothing());
  loop.Run();
  EXPECT_FALSE(base::PathExists(dir().GetPath().AppendASCII(kGuid)));
}

}  // namespace download
