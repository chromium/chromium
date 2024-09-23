// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/cluster_server_proxy.h"

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/commerce/core/commerce_types.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/pref_names.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/endpoint_fetcher/mock_endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace commerce {
namespace {

const char kTestServerUrl[] =
    "https://memex-pa.googleapis.com/v1/shopping/products:related";
const uint64_t kProduct1Id = 123u;
const uint64_t kProduct2Id = 234u;

const std::string kSimpleResponse = R"(
    {
      "productIds": [
        {
          "type": "TYPE_UNSPECIFIED",
          "identifier": "123"
        },
        {
          "type": "GLOBAL_PRODUCT_CLUSTER_ID",
          "identifier": "234"
        }
      ]
    })";

std::string GetPostData() {
  std::string post_data_string = R"(
    {
      "productIds": [
        {
          "identifier": "123",
          "type": "GLOBAL_PRODUCT_CLUSTER_ID"
        },
        {
          "identifier": "234",
          "type": "GLOBAL_PRODUCT_CLUSTER_ID"
        }
      ]
    })";
  std::optional<base::Value::Dict> dict =
      base::JSONReader::ReadDict(post_data_string);
  std::string post_data;
  base::JSONWriter::Write(dict.value(), &post_data);
  return post_data;
}

class FakeClusterServerProxy : public ClusterServerProxy {
 public:
  FakeClusterServerProxy(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : ClusterServerProxy(identity_manager, url_loader_factory) {}
  FakeClusterServerProxy(const FakeClusterServerProxy&) = delete;
  FakeClusterServerProxy operator=(const FakeClusterServerProxy&) = delete;
  ~FakeClusterServerProxy() override = default;
  MOCK_METHOD(std::unique_ptr<EndpointFetcher>,
              CreateEndpointFetcher,
              (const GURL& url, const std::string& post_data),
              (override));
};

class ClusterServerProxyTest : public testing::Test {
 protected:
  void SetUp() override {
    fetcher_ = std::make_unique<MockEndpointFetcher>();
    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    server_proxy_ = std::make_unique<FakeClusterServerProxy>(
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
  std::unique_ptr<FakeClusterServerProxy> server_proxy_;
};

TEST_F(ClusterServerProxyTest, TestGetComparableProducts) {
  fetcher_->SetFetchResponse(kSimpleResponse);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kTestServerUrl), GetPostData()))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->GetComparableProducts(
      std::vector<uint64_t>{kProduct1Id, kProduct2Id},
      base::BindOnce([](const std::vector<uint64_t>& product_cluster_ids) {
        ASSERT_EQ(product_cluster_ids.size(), 1u);
        ASSERT_EQ(product_cluster_ids[0], kProduct2Id);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(ClusterServerProxyTest, TestGetComparableProductsWithServerError) {
  fetcher_->SetFetchResponse(kSimpleResponse, net::HTTP_NOT_FOUND);
  EXPECT_CALL(*server_proxy_,
              CreateEndpointFetcher(GURL(kTestServerUrl), GetPostData()))
      .Times(1);
  base::RunLoop run_loop;
  server_proxy_->GetComparableProducts(
      std::vector<uint64_t>{kProduct1Id, kProduct2Id},
      base::BindOnce([](const std::vector<uint64_t>& product_cluster_ids) {
        ASSERT_EQ(product_cluster_ids.size(), 0u);
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace
}  // namespace commerce
