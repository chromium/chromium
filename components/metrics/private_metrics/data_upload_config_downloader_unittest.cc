// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/data_upload_config_downloader.h"

#include <string>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/federated_compute/src/fcp/protos/confidentialcompute/data_upload_config.pb.h"

namespace metrics::private_metrics {

namespace {

inline constexpr char kDataUploadConfigGstaticUrl[] =
    "https://www.gstatic.com/chrome/private-metrics/data-upload-config";

void AddValidDataUploadConfigResponse(
    network::TestURLLoaderFactory& test_url_loader_factory) {
  test_url_loader_factory.AddResponse(
      kDataUploadConfigGstaticUrl, []() -> std::string {
        google::internal::federatedcompute::v1::ConfidentialEncryptionConfig
            encryption_config;
        *encryption_config.mutable_public_key() = "test_public_key";

        fcp::confidentialcompute::DataUploadConfig config;
        *config.mutable_confidential_encryption_config() = encryption_config;

        return config.SerializeAsString();
      }());
}

void AddInvalidDataUploadConfigResponse(
    network::TestURLLoaderFactory& test_url_loader_factory) {
  test_url_loader_factory.AddResponse(
      kDataUploadConfigGstaticUrl, []() -> std::string {
        google::internal::federatedcompute::v1::ConfidentialEncryptionConfig
            encryption_config;
        *encryption_config.mutable_public_key() = "test_public_key";

        // Return the wrong type of protocol buffer.
        return encryption_config.SerializeAsString();
      }());
}

}  // namespace

class DataUploadConfigDownloaderTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(DataUploadConfigDownloaderTest, ReturnsValidDataUploadConfig) {
  network::TestURLLoaderFactory test_url_loader_factory;
  AddValidDataUploadConfigResponse(test_url_loader_factory);

  DataUploadConfigDownloader downloader(
      scoped_refptr<network::SharedURLLoaderFactory>(
          test_url_loader_factory.GetSafeWeakWrapper()));
  fcp::confidentialcompute::DataUploadConfig config;

  base::RunLoop run_loop;
  downloader.FetchDataUploadConfig(base::BindLambdaForTesting(
      [&](std::optional<fcp::confidentialcompute::DataUploadConfig>
              data_upload_config) {
        config = std::move(*data_upload_config);
        run_loop.Quit();
      }));
  EXPECT_NE(downloader.GetPendingRequestForTesting(), nullptr);
  run_loop.Run();

  EXPECT_EQ(config.confidential_encryption_config().public_key(),
            "test_public_key");
  EXPECT_EQ(downloader.GetPendingRequestForTesting(), nullptr);
}

TEST_F(DataUploadConfigDownloaderTest, ReturnsValidDataUploadConfigWithRetry) {
  network::TestURLLoaderFactory test_url_loader_factory;

  DataUploadConfigDownloader downloader(
      scoped_refptr<network::SharedURLLoaderFactory>(
          test_url_loader_factory.GetSafeWeakWrapper()));
  fcp::confidentialcompute::DataUploadConfig config;

  base::RunLoop run_loop;
  auto lambda = base::BindLambdaForTesting(
      [&](std::optional<fcp::confidentialcompute::DataUploadConfig>
              data_upload_config) {
        config = std::move(*data_upload_config);
        run_loop.Quit();
      });

  // First call to endpoint should result in a 5XX error.
  downloader.FetchDataUploadConfig(lambda);
  EXPECT_NE(downloader.GetPendingRequestForTesting(), nullptr);
  test_url_loader_factory.SimulateResponseForPendingRequest(
      kDataUploadConfigGstaticUrl, "", net::HTTP_INTERNAL_SERVER_ERROR);

  // Second call to endpoint should be successful.
  AddValidDataUploadConfigResponse(test_url_loader_factory);
  downloader.FetchDataUploadConfig(lambda);
  EXPECT_NE(downloader.GetPendingRequestForTesting(), nullptr);
  run_loop.Run();

  EXPECT_EQ(config.confidential_encryption_config().public_key(),
            "test_public_key");
  EXPECT_EQ(downloader.GetPendingRequestForTesting(), nullptr);
}

TEST_F(DataUploadConfigDownloaderTest, ReturnsInvalidProtocolBuffer) {
  network::TestURLLoaderFactory test_url_loader_factory;
  AddInvalidDataUploadConfigResponse(test_url_loader_factory);

  DataUploadConfigDownloader downloader(
      scoped_refptr<network::SharedURLLoaderFactory>(
          test_url_loader_factory.GetSafeWeakWrapper()));
  fcp::confidentialcompute::DataUploadConfig config;

  bool optional_empty = false;
  base::RunLoop run_loop;
  downloader.FetchDataUploadConfig(base::BindLambdaForTesting(
      [&](std::optional<fcp::confidentialcompute::DataUploadConfig>
              data_upload_config) {
        optional_empty = !data_upload_config.has_value();
        run_loop.Quit();
      }));
  EXPECT_NE(downloader.GetPendingRequestForTesting(), nullptr);
  run_loop.Run();

  EXPECT_EQ(optional_empty, true);
  EXPECT_EQ(downloader.GetPendingRequestForTesting(), nullptr);
}

TEST_F(DataUploadConfigDownloaderTest, ReturnsEmptyResponse) {
  network::TestURLLoaderFactory test_url_loader_factory;
  test_url_loader_factory.AddResponse(kDataUploadConfigGstaticUrl,
                                      []() -> std::string { return ""; }());

  DataUploadConfigDownloader downloader(
      scoped_refptr<network::SharedURLLoaderFactory>(
          test_url_loader_factory.GetSafeWeakWrapper()));

  bool optional_empty = false;
  base::RunLoop run_loop;
  downloader.FetchDataUploadConfig(base::BindLambdaForTesting(
      [&](std::optional<fcp::confidentialcompute::DataUploadConfig>
              data_upload_config) {
        optional_empty = !data_upload_config.has_value();
        run_loop.Quit();
      }));
  EXPECT_NE(downloader.GetPendingRequestForTesting(), nullptr);
  run_loop.Run();

  EXPECT_EQ(optional_empty, true);
  EXPECT_EQ(downloader.GetPendingRequestForTesting(), nullptr);
}

}  // namespace metrics::private_metrics
