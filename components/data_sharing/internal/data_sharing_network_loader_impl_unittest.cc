// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_network_loader_impl.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/data_sharing/public/data_sharing_network_loader.h"
#include "components/data_sharing/public/group_data.h"
#include "components/endpoint_fetcher/mock_endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_sharing {

const std::string kExpectedResponse = "foo";

class MockDataSharingNetworkLoaderImpl : public DataSharingNetworkLoaderImpl {
 public:
  MockDataSharingNetworkLoaderImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager)
      : DataSharingNetworkLoaderImpl(url_loader_factory, identity_manager) {}
  MockDataSharingNetworkLoaderImpl(const MockDataSharingNetworkLoaderImpl&) =
      delete;
  MockDataSharingNetworkLoaderImpl operator=(
      const MockDataSharingNetworkLoaderImpl&) = delete;
  ~MockDataSharingNetworkLoaderImpl() override = default;

  MOCK_METHOD(std::unique_ptr<EndpointFetcher>,
              CreateEndpointFetcher,
              (const GURL& url,
               const std::vector<std::string>& scopes,
               const std::string& post_data,
               const net::NetworkTrafficAnnotationTag& annotation_tag),
              (override));
};

class DataSharingNetworkLoaderImplTest : public testing::Test {
 public:
  DataSharingNetworkLoaderImplTest() = default;
  ~DataSharingNetworkLoaderImplTest() override = default;

  void SetUp() override {
    fetcher_ = std::make_unique<MockEndpointFetcher>();
    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    data_sharing_network_loader_ =
        std::make_unique<MockDataSharingNetworkLoaderImpl>(
            std::move(test_url_loader_factory),
            identity_test_env_.identity_manager());
    ON_CALL(*data_sharing_network_loader_, CreateEndpointFetcher)
        .WillByDefault([this]() { return std::move(fetcher_); });
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockEndpointFetcher> fetcher_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<MockDataSharingNetworkLoaderImpl>
      data_sharing_network_loader_;
};

TEST_F(DataSharingNetworkLoaderImplTest, BadHttpStatusCode) {
  fetcher_->SetFetchResponse(std::string(), net::HTTP_BAD_REQUEST);
  base::RunLoop run_loop;
  data_sharing_network_loader_->LoadUrl(
      GURL("http://foo.com"), std::vector<std::string>(), std::string(),
      TRAFFIC_ANNOTATION_FOR_TESTS,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::unique_ptr<DataSharingNetworkLoader::LoadResult> response) {
            ASSERT_EQ(response->status,
                      DataSharingNetworkLoader::NetworkLoaderStatus::
                          kTransientFailure);
            ASSERT_TRUE(response->result_bytes.empty());
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(DataSharingNetworkLoaderImplTest, CallbackRunOnUrlResponse) {
  fetcher_->SetFetchResponse(kExpectedResponse);
  base::RunLoop run_loop;
  data_sharing_network_loader_->LoadUrl(
      GURL("http://foo.com"), std::vector<std::string>(), std::string(),
      TRAFFIC_ANNOTATION_FOR_TESTS,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::unique_ptr<DataSharingNetworkLoader::LoadResult> response) {
            ASSERT_EQ(response->status,
                      DataSharingNetworkLoader::NetworkLoaderStatus::kSuccess);
            ASSERT_EQ(response->result_bytes, kExpectedResponse);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace data_sharing
