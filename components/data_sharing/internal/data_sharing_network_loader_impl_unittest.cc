// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/data_sharing_network_loader_impl.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "components/data_sharing/public/data_sharing_network_loader.h"
#include "components/data_sharing/public/group_data.h"
#include "components/endpoint_fetcher/mock_endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

using endpoint_fetcher::MockEndpointFetcher;

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

  MOCK_METHOD(std::unique_ptr<endpoint_fetcher::EndpointFetcher>,
              CreateEndpointFetcher,
              (const GURL& url,
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
      GURL("http://foo.com"), std::string(),
      DataSharingNetworkLoader::DataSharingRequestType::kTestRequest,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::unique_ptr<DataSharingNetworkLoader::LoadResult> response) {
            ASSERT_EQ(response->status,
                      DataSharingNetworkLoader::NetworkLoaderStatus::
                          kTransientFailure);
            ASSERT_TRUE(response->result_bytes.empty());
            ASSERT_EQ(response->network_error_code, 400);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(DataSharingNetworkLoaderImplTest, CallbackRunOnUrlResponse) {
  fetcher_->SetFetchResponse(kExpectedResponse);
  base::RunLoop run_loop;
  data_sharing_network_loader_->LoadUrl(
      GURL("http://foo.com"), std::string(),
      DataSharingNetworkLoader::DataSharingRequestType::kTestRequest,
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

namespace {

using DataSharingRequestType = DataSharingNetworkLoader::DataSharingRequestType;

struct TrafficAnnotationTestCase {
  template <size_t N>
  consteval TrafficAnnotationTestCase(DataSharingRequestType request_type,
                                      const char (&annotation_unique_id)[N],
                                      const char* test_name)
      : request_type(request_type),
        expected_annotation_hash(
            net::internal::ComputeAnnotationHash(annotation_unique_id)),
        test_name(test_name) {}

  DataSharingRequestType request_type;
  int32_t expected_annotation_hash;
  const char* test_name;
};

}  // namespace

class DataSharingNetworkLoaderImplTrafficAnnotationTest
    : public DataSharingNetworkLoaderImplTest,
      public testing::WithParamInterface<TrafficAnnotationTestCase> {};

TEST_P(DataSharingNetworkLoaderImplTrafficAnnotationTest,
       RequestTypeMapsToTrafficAnnotation) {
  fetcher_->SetFetchResponse(kExpectedResponse);
  int32_t annotation_hash = net::internal::TRAFFIC_ANNOTATION_UNINITIALIZED;
  ON_CALL(*data_sharing_network_loader_, CreateEndpointFetcher)
      .WillByDefault(
          [this, &annotation_hash](
              const GURL&, const std::string&,
              const net::NetworkTrafficAnnotationTag& annotation_tag) {
            annotation_hash = annotation_tag.unique_id_hash_code;
            return std::move(fetcher_);
          });

  data_sharing_network_loader_->LoadUrl(GURL("http://foo.com"), std::string(),
                                        GetParam().request_type,
                                        base::DoNothing());

  EXPECT_EQ(annotation_hash, GetParam().expected_annotation_hash);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DataSharingNetworkLoaderImplTrafficAnnotationTest,
    testing::Values(
        TrafficAnnotationTestCase(DataSharingRequestType::kCreateGroup,
                                  "data_sharing_service_create_group",
                                  "CreateGroup"),
        TrafficAnnotationTestCase(DataSharingRequestType::kReadGroups,
                                  "data_sharing_service_read_groups",
                                  "ReadGroups"),
        TrafficAnnotationTestCase(DataSharingRequestType::kReadAllGroups,
                                  "data_sharing_service_read_groups",
                                  "ReadAllGroups"),
        TrafficAnnotationTestCase(DataSharingRequestType::kDeleteGroups,
                                  "data_sharing_service_delete_groups",
                                  "DeleteGroups"),
        TrafficAnnotationTestCase(DataSharingRequestType::kUpdateGroup,
                                  "data_sharing_service_update_group",
                                  "UpdateGroup"),
        TrafficAnnotationTestCase(DataSharingRequestType::kLookup,
                                  "data_sharing_service_lookup",
                                  "Lookup"),
        TrafficAnnotationTestCase(DataSharingRequestType::kLeaveGroup,
                                  "data_sharing_service_leave_group",
                                  "LeaveGroup"),
        TrafficAnnotationTestCase(DataSharingRequestType::kBlockPerson,
                                  "data_sharing_service_block_person",
                                  "BlockPerson"),
        TrafficAnnotationTestCase(DataSharingRequestType::kJoinGroup,
                                  "data_sharing_service_join_group",
                                  "JoinGroup"),
        // Request types without a dedicated annotation fall back to the read
        // groups annotation. See crbug.com/375594409.
        TrafficAnnotationTestCase(DataSharingRequestType::kWarmup,
                                  "data_sharing_service_read_groups",
                                  "Warmup")),
    [](const testing::TestParamInfo<TrafficAnnotationTestCase>& info) {
      return std::string(info.param.test_name);
    });

}  // namespace data_sharing
