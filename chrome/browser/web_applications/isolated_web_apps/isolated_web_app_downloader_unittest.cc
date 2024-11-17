// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_downloader.h"

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;

class IsolatedWebAppDownloaderTest : public ::testing::Test {
 public:
  IsolatedWebAppDownloaderTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_factory_)) {}

  void SetUp() override { CHECK(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath bundle_path() {
    return temp_dir_.GetPath().Append(FILE_PATH_LITERAL("bundle.swbn"));
  }

  GURL download_url() { return GURL("https://example.com/bundle.swbn"); }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  base::ScopedTempDir temp_dir_;
};
}  // namespace

TEST_F(IsolatedWebAppDownloaderTest, SuccessfulDownload) {
  test_factory_.AddResponse(download_url().spec(), "test bundle content",
                            net::HttpStatusCode::HTTP_OK);

  base::test::TestFuture<int32_t> future;
  auto downloader = IsolatedWebAppDownloader::CreateAndStartDownloading(
      download_url(), bundle_path(), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      shared_url_loader_factory_, future.GetCallback());

  EXPECT_THAT(future.Take(), Eq(net::OK));
  EXPECT_THAT(base::PathExists(bundle_path()), IsTrue());

  std::string file_contents;
  EXPECT_THAT(base::ReadFileToString(bundle_path(), &file_contents), IsTrue());
  EXPECT_THAT(file_contents, Eq("test bundle content"));
}

TEST_F(IsolatedWebAppDownloaderTest, FailedDownload) {
  test_factory_.AddResponse(download_url().spec(), "",
                            net::HttpStatusCode::HTTP_NOT_FOUND);

  base::test::TestFuture<int32_t> future;
  auto downloader = IsolatedWebAppDownloader::CreateAndStartDownloading(
      download_url(), bundle_path(), PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
      shared_url_loader_factory_, future.GetCallback());

  EXPECT_THAT(future.Take(), Eq(net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE));
  EXPECT_THAT(base::PathExists(bundle_path()), IsFalse());
}

TEST_F(IsolatedWebAppDownloaderTest,
       SuccessfulPartialDownloadServerIgnoresRange) {
  test_factory_.AddResponse(download_url().spec(), std::string(10 * 1024, 'x'),
                            net::HttpStatusCode::HTTP_OK);

  base::test::TestFuture<std::optional<std::string>> future;
  auto downloader =
      IsolatedWebAppDownloader::Create(shared_url_loader_factory_);
  downloader->DownloadInitialBytes(download_url(),
                                   PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                                   future.GetCallback());

  EXPECT_THAT(future.Take(), Eq(std::string(8 * 1024, 'x')));
}

TEST_F(IsolatedWebAppDownloaderTest, SuccessfulPartialDownload) {
  test_factory_.AddResponse(download_url().spec(), std::string(8 * 1024, 'x'),
                            net::HttpStatusCode::HTTP_PARTIAL_CONTENT);

  base::test::TestFuture<std::optional<std::string>> future;
  auto downloader =
      IsolatedWebAppDownloader::Create(shared_url_loader_factory_);
  downloader->DownloadInitialBytes(download_url(),
                                   PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                                   future.GetCallback());

  EXPECT_THAT(future.Take(), Eq(std::string(8 * 1024, 'x')));
}

TEST_F(IsolatedWebAppDownloaderTest, SuccessfulPartialDownloadOfSmallContent) {
  test_factory_.AddResponse(download_url().spec(), "cthulhu",
                            net::HttpStatusCode::HTTP_OK);

  base::test::TestFuture<std::optional<std::string>> future;
  auto downloader =
      IsolatedWebAppDownloader::Create(shared_url_loader_factory_);
  downloader->DownloadInitialBytes(download_url(),
                                   PARTIAL_TRAFFIC_ANNOTATION_FOR_TESTS,
                                   future.GetCallback());

  EXPECT_THAT(future.Take(), Eq("cthulhu"));
}

}  // namespace web_app
