// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/carrier_lock/provisioning_config_fetcher_impl.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::carrier_lock {

namespace {
const char kProvisioningUrl[] =
    "https://afwprovisioning-pa.googleapis.com"
    "/v1/get_device_provisioning_record";

const char kManufacturer[] = "Google";
const char kModel[] = "Pixel 20";
const char kSerial[] = "5CD203JBP8";
const char kImei[] = "862146050085792";
const char kFcmToken[] =
    "cDd5F3NbW-M:APA91bEBRHrtTr2UYGeRpB5EPwnmqN4_"
    "U6oKTgBMl15Zd1ukADsa4FbIxUp6570ZCUZwFRmwWs3bhbc8EfJOL8yVxB_5ohpWfYJu33_"
    "Gzbvk9YxtmUjmO9CdS_C7Is3QmU2KEPonDGeF";
const char kConfigResponse[] =
    "ewogICJkZXZpY2VQcm92aXNpb25pbmdSZWNvcmQiOiB7CiAgICAiZGV2aWNlUHJvdmlzaW9uaW"
    "5nQ29uZmlnIjogIkNpOEtEek0xTVRZd01qZ3dNemd3TnpNM054b0tOVU5FTWpBelNrSlFPRG9H"
    "UjI5dloyeGxRZ2hRYVhobGJDQXlNQklNQ043NGhLSUdFSUNWaDU4Q0dwa0NDaHdLQXpNeE1SSU"
    "RORGd3S2hCQ1FVVXdNREF3TURBd01EQXdNREF3RWlNdmRHOXdhV056TDFORlExUkpUMDVmVkZs"
    "UVJWOVRTVTFmVEU5RFN5MDFNREEwTlJvL0NnZFVaWE4wYVc1bkVqUm9kSFJ3Y3pvdkwzTjFjSE"
    "J2Y25RdVoyOXZaMnhsTG1OdmJTOXdhWGhsYkhCb2IyNWxMMkZ1YzNkbGNpODNNVEEzTVRnNFFw"
    "SUJBRkFBRmdCZ0FBWXpNVEUwT0RBQVl3QUl1dUFBQUFBQUFBQUFVZ0FCQUFCVEFBRUFBRlFBQ0"
    "FBQUFBQmtRVHhlQUZVQUR6TTFNVFl3TWpnd016Z3dOek0zTndCV0FBRUJBRmNBUmpCRUFpQlht"
    "QllrNVJSNENtTDVqRWF5eFliY0ZZQyszVDRiK1prRWZocldMckR6QlFJZ096YUZQd3dMMnIrNV"
    "dRaEliNmpEeHZhZnhaSFRSN1BLQkdPUlhRemZvdm9xQUE9PSIKICB9Cn0K";
const char kConfigResponseEmpty[] =
    "ewogICJkZXZpY2VQcm92aXNpb25pbmdSZWNvcmQiOiB7CiAgfQp9Cg==";
const char kConfigResponseInvalid[] =
    "ewogICJkZXZpY2VQcm92aXNpb25pbmdSZWNvcmQiOiB7CiAgICAiZGV2aWNlUHJvdmlzaW9uaW"
    "5nQ29uZmlnIjogIkNpOEtEek0xTVRZd01qZ3dNIgogIH0KfQo=";
const char kConfigTopic[] = "/topics/SECTION_TYPE_SIM_LOCK-50045";
}  // namespace

class ProvisioningConfigFetcherTest : public testing::Test {
 public:
  ProvisioningConfigFetcherTest() = default;
  ProvisioningConfigFetcherTest(const ProvisioningConfigFetcherTest&) = delete;
  ProvisioningConfigFetcherTest& operator=(
      const ProvisioningConfigFetcherTest&) = delete;
  ~ProvisioningConfigFetcherTest() override = default;

 protected:
  // testing::Test:
  void SetUp() override {
    shared_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    config_ = std::make_unique<ProvisioningConfigFetcherImpl>(shared_factory_);
  }

  void TearDown() override { config_.reset(); }

  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<ProvisioningConfigFetcher> config_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ProvisioningConfigFetcherTest, CarrierLockRequestConfigSuccess) {
  std::string response;

  // Send configuration request
  base::test::TestFuture<Result> future;
  config_->RequestConfig(kSerial, kImei, kManufacturer, kModel, kFcmToken,
                         future.GetCallback());

  // Send fake response
  base::Base64Decode(kConfigResponse, &response);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kProvisioningUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), response));

  // Wait for callback
  EXPECT_EQ(Result::kSuccess, future.Get());
  EXPECT_EQ(kConfigTopic, config_->GetFcmTopic());
}

TEST_F(ProvisioningConfigFetcherTest, CarrierLockRequestConfigTwice) {
  std::string response;

  // Send configuration request twice
  base::test::TestFuture<Result> future;
  config_->RequestConfig(kSerial, kImei, kManufacturer, kModel, kFcmToken,
                         future.GetCallback());
  config_->RequestConfig(kSerial, kImei, kManufacturer, kModel, kFcmToken,
                         future.GetCallback());

  // Wait for callback
  EXPECT_EQ(Result::kHandlerBusy, future.Get());
  EXPECT_EQ(std::string(), config_->GetFcmTopic());
}

TEST_F(ProvisioningConfigFetcherTest, CarrierLockRequestConfigInvalidResponse) {
  base::test::TestFuture<Result> future;

  // Send configuration request
  config_->RequestConfig(kSerial, kImei, kManufacturer, kModel, kFcmToken,
                         future.GetCallback());

  // Send empty response
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kProvisioningUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), std::string()));

  // Wait for callback
  EXPECT_EQ(Result::kInvalidResponse, future.Get());
  EXPECT_EQ(std::string(), config_->GetFcmTopic());
}

TEST_F(ProvisioningConfigFetcherTest, CarrierLockRequestConfigNoConfig) {
  std::string response;

  // Send configuration request
  base::test::TestFuture<Result> future;
  config_->RequestConfig(kSerial, kImei, kManufacturer, kModel, kFcmToken,
                         future.GetCallback());

  // Send response without configuration
  base::Base64Decode(kConfigResponseEmpty, &response);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kProvisioningUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), response));

  // Wait for callback
  EXPECT_EQ(Result::kNoLockConfiguration, future.Get());
  EXPECT_EQ(std::string(), config_->GetFcmTopic());
}

TEST_F(ProvisioningConfigFetcherTest, CarrierLockRequestConfigInvalidConfig) {
  std::string response;

  // Send configuration request
  base::test::TestFuture<Result> future;
  config_->RequestConfig(kSerial, kImei, kManufacturer, kModel, kFcmToken,
                         future.GetCallback());

  // Send response with invalid configuration
  base::Base64Decode(kConfigResponseInvalid, &response);
  EXPECT_TRUE(test_url_loader_factory_.SimulateResponseForPendingRequest(
      GURL(kProvisioningUrl), network::URLLoaderCompletionStatus(net::OK),
      network::CreateURLResponseHead(net::HTTP_OK), response));

  // Wait for callback
  EXPECT_EQ(Result::kInvalidConfiguration, future.Get());
  EXPECT_EQ(std::string(), config_->GetFcmTopic());
}

}  // namespace ash::carrier_lock
