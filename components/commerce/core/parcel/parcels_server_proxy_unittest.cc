// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <string>
#include <unordered_map>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/parcel/parcels_server_proxy.h"
#include "components/commerce/core/proto/parcel.pb.h"
#include "components/endpoint_fetcher/mock_endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;

namespace {

const char kParcelsBaseUrl[] =
    "https://memex-pa.googleapis.com/v1/shopping/parcels";
const char kParcelsStatusUrl[] =
    "https://memex-pa.googleapis.com/v1/shopping/parcels:status";
const char kParcelsUntrackUrl[] =
    "https://memex-pa.googleapis.com/v1/shopping/parcels:untrack";

const std::string kExpectedGetParcelStatusPostData =
    "{\"parcelIds\":[{\"carrier\":2,\"trackingId\":\"xyz\"}]}";
const std::string kResponseSucceeded =
    "{ \"parcelStatus\": [{\"parcelIdentifier\": {\"trackingId\": \"xyz\","
    "\"carrier\": \"FEDEX\"}, \"parcelState\": \"PICKED_UP\", \"trackingUrl\": "
    "\"www.foo.com\","
    "\"estimatedDeliveryDate\": \"2023-10-11\"}]}";
const std::string kExpectedStartTrackingPostData =
    "{\"parcelIds\":[{\"carrier\":2,\"trackingId\":\"xyz\"}],"
    "\"sourcePageDomain\":\"www.abc.com\"}";
const std::string kExpectedStopTrackingPostData =
    "{\"parcelIds\":[{\"carrier\":2,\"trackingId\":\"xyz\"}]}";
const std::string kTestTrackingUrl = "www.foo.com";
const std::string kTestTrackingId = "xyz";
const std::string kTestSourcePageDomain = "www.abc.com";

std::vector<commerce::ParcelIdentifier> GetTestParcelIdentifiers() {
  commerce::ParcelIdentifier identifier;
  identifier.set_tracking_id("xyz");
  identifier.set_carrier(commerce::ParcelIdentifier::UPS);
  return std::vector<commerce::ParcelIdentifier>{identifier};
}

int64_t GetExpectedDeliveryTimeUsec() {
  base::Time delivery;
  CHECK(base::Time::FromString("11-OCT-2023 00:00:00", &delivery));
  return delivery.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

}  // namespace

namespace commerce {

class MockParcelsServerProxy : public ParcelsServerProxy {
 public:
  MockParcelsServerProxy(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : ParcelsServerProxy(identity_manager, std::move(url_loader_factory)) {}
  MockParcelsServerProxy(const MockParcelsServerProxy&) = delete;
  MockParcelsServerProxy operator=(const MockParcelsServerProxy&) = delete;
  ~MockParcelsServerProxy() override = default;

  MOCK_METHOD(std::unique_ptr<EndpointFetcher>,
              CreateEndpointFetcher,
              (const GURL& url,
               const std::string& http_method,
               const std::string& post_data,
               const net::NetworkTrafficAnnotationTag traffic_annotation),
              (override));
};

class ParcelsServerProxyTest : public testing::Test {
 public:
  ParcelsServerProxyTest() = default;
  ~ParcelsServerProxyTest() override = default;

  void SetUp() override {
    fetcher_ = std::make_unique<MockEndpointFetcher>();
    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    server_proxy_ = std::make_unique<MockParcelsServerProxy>(
        identity_test_env_.identity_manager(),
        std::move(test_url_loader_factory));
    ON_CALL(*server_proxy_, CreateEndpointFetcher).WillByDefault([this]() {
      return std::move(fetcher_);
    });
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  std::unique_ptr<MockEndpointFetcher> fetcher_;
  std::unique_ptr<MockParcelsServerProxy> server_proxy_;
  base::HistogramTester histogram_tester_;
};

TEST_F(ParcelsServerProxyTest, TestGetParcelStatus) {
  fetcher_->SetFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kParcelsStatusUrl), kPostHttpMethod,
                                    kExpectedGetParcelStatusPostData, _))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->GetParcelStatus(
      GetTestParcelIdentifiers(),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success,
             std::unique_ptr<std::vector<ParcelStatus>> parcel_status) {
            ASSERT_TRUE(success);
            ASSERT_EQ(1, static_cast<int>(parcel_status->size()));
            auto status = (*parcel_status)[0];
            ASSERT_EQ(ParcelStatus::PICKED_UP, status.parcel_state());
            ASSERT_EQ(kTestTrackingUrl, status.tracking_url());
            ASSERT_EQ(GetExpectedDeliveryTimeUsec(),
                      status.estimated_delivery_time_usec());
            ASSERT_EQ(kTestTrackingId,
                      status.parcel_identifier().tracking_id());
            ASSERT_EQ(commerce::ParcelIdentifier::FEDEX,
                      status.parcel_identifier().carrier());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      "Commerce.ParcelTracking.GetParcelStatus.RequestStatus",
      ParcelRequestStatus::kSuccess, 1);
}

TEST_F(ParcelsServerProxyTest, TestGetParcelStatusWithErrorResponse) {
  fetcher_->SetFetchResponse("{1:2}");
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kParcelsStatusUrl), kPostHttpMethod,
                                    kExpectedGetParcelStatusPostData, _))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->GetParcelStatus(
      GetTestParcelIdentifiers(),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success,
             std::unique_ptr<std::vector<ParcelStatus>> parcel_status) {
            ASSERT_FALSE(success);
            ASSERT_EQ(0, static_cast<int>(parcel_status->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      "Commerce.ParcelTracking.GetParcelStatus.RequestStatus",
      ParcelRequestStatus::kServerReponseParsingError, 1);
}

TEST_F(ParcelsServerProxyTest, TestGetParcelStatusWithoutTrackingId) {
  base::RunLoop run_loop;
  server_proxy_->GetParcelStatus(
      std::vector<ParcelIdentifier>{ParcelIdentifier()},
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success,
             std::unique_ptr<std::vector<ParcelStatus>> parcel_status) {
            ASSERT_FALSE(success);
            ASSERT_EQ(0, static_cast<int>(parcel_status->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      "Commerce.ParcelTracking.GetParcelStatus.RequestStatus",
      ParcelRequestStatus::kInvalidParcelIdentifiers, 1);
}

TEST_F(ParcelsServerProxyTest, TestGetParcelStatusWithServerError) {
  fetcher_->SetFetchResponse(kResponseSucceeded, net::HTTP_NOT_FOUND);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kParcelsStatusUrl), kPostHttpMethod,
                                    kExpectedGetParcelStatusPostData, _))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->GetParcelStatus(
      GetTestParcelIdentifiers(),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success,
             std::unique_ptr<std::vector<ParcelStatus>> parcel_status) {
            ASSERT_FALSE(success);
            ASSERT_EQ(0, static_cast<int>(parcel_status->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      "Commerce.ParcelTracking.GetParcelStatus.RequestStatus",
      ParcelRequestStatus::kServerError, 1);
}

TEST_F(ParcelsServerProxyTest, TestStartTrackingParcelsWithServerError) {
  fetcher_->SetFetchResponse(kResponseSucceeded, net::HTTP_NOT_FOUND);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kParcelsBaseUrl), kPostHttpMethod,
                                    kExpectedStartTrackingPostData, _))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->StartTrackingParcels(
      GetTestParcelIdentifiers(), kTestSourcePageDomain,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success,
             std::unique_ptr<std::vector<ParcelStatus>> parcel_status) {
            ASSERT_FALSE(success);
            ASSERT_EQ(0, static_cast<int>(parcel_status->size()));
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      "Commerce.ParcelTracking.StartTrackingParcels.RequestStatus",
      ParcelRequestStatus::kServerError, 1);
}

TEST_F(ParcelsServerProxyTest, TestStartTrackingParcels) {
  fetcher_->SetFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kParcelsBaseUrl), kPostHttpMethod,
                                    kExpectedStartTrackingPostData, _))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->StartTrackingParcels(
      GetTestParcelIdentifiers(), kTestSourcePageDomain,
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success,
             std::unique_ptr<std::vector<ParcelStatus>> parcel_status) {
            ASSERT_TRUE(success);
            ASSERT_EQ(1, static_cast<int>(parcel_status->size()));
            auto status = (*parcel_status)[0];
            ASSERT_EQ(ParcelStatus::PICKED_UP, status.parcel_state());
            ASSERT_EQ(kTestTrackingUrl, status.tracking_url());
            ASSERT_EQ(GetExpectedDeliveryTimeUsec(),
                      status.estimated_delivery_time_usec());
            ASSERT_EQ(kTestTrackingId,
                      status.parcel_identifier().tracking_id());
            ASSERT_EQ(commerce::ParcelIdentifier::FEDEX,
                      status.parcel_identifier().carrier());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      "Commerce.ParcelTracking.StartTrackingParcels.RequestStatus",
      ParcelRequestStatus::kSuccess, 1);
}

TEST_F(ParcelsServerProxyTest, TestStopTrackingParcel) {
  fetcher_->SetFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(
                  GURL(std::string(kParcelsBaseUrl) + "/" + kTestTrackingId),
                  kDeleteHttpMethod, std::string(), _))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->StopTrackingParcel(
      kTestTrackingId, base::BindOnce(
                           [](base::RunLoop* run_loop, bool success) {
                             ASSERT_TRUE(success);
                             run_loop->Quit();
                           },
                           &run_loop));
  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      "Commerce.ParcelTracking.Unknown.RequestStatus",
      ParcelRequestStatus::kSuccess, 1);
}

TEST_F(ParcelsServerProxyTest, TestStopTrackingParcels) {
  fetcher_->SetFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kParcelsUntrackUrl), kPostHttpMethod,
                                    kExpectedStopTrackingPostData, _))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->StopTrackingParcels(
      GetTestParcelIdentifiers(),
      base::BindOnce(
          [](base::RunLoop* run_loop, bool success) {
            ASSERT_TRUE(success);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      "Commerce.ParcelTracking.StopTrackingParcels.RequestStatus",
      ParcelRequestStatus::kSuccess, 1);
}

TEST_F(ParcelsServerProxyTest, TestStopTrackingAllParcels) {
  fetcher_->SetFetchResponse(kResponseSucceeded);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kParcelsBaseUrl), kDeleteHttpMethod,
                                    std::string(), _))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->StopTrackingAllParcels(base::BindOnce(
      [](base::RunLoop* run_loop, bool success) {
        ASSERT_TRUE(success);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      "Commerce.ParcelTracking.StopTrackingAllParcels.RequestStatus",
      ParcelRequestStatus::kSuccess, 1);
}

TEST_F(ParcelsServerProxyTest, TestStopTrackingAllParcelsWithServerError) {
  fetcher_->SetFetchResponse(kResponseSucceeded, net::HTTP_NOT_FOUND);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kParcelsBaseUrl), kDeleteHttpMethod,
                                    std::string(), _))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->StopTrackingAllParcels(base::BindOnce(
      [](base::RunLoop* run_loop, bool success) {
        ASSERT_FALSE(success);
        run_loop->Quit();
      },
      &run_loop));
  run_loop.Run();
  histogram_tester_.ExpectBucketCount(
      "Commerce.ParcelTracking.StopTrackingAllParcels.RequestStatus",
      ParcelRequestStatus::kServerError, 1);
}

}  // namespace commerce
