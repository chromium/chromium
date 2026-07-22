// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_network_service_impl.h"

#include <memory>
#include <string>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace notebooks {

namespace {

class TestNotebooksNetworkServiceImpl : public NotebooksNetworkServiceImpl {
 public:
  TestNotebooksNetworkServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager)
      : NotebooksNetworkServiceImpl(url_loader_factory, identity_manager) {}

  // Expose the protected method for testing.
  using NotebooksNetworkServiceImpl::CreateEndpointFetcher;
};

class NotebooksNetworkServiceImplTest : public testing::Test {
 public:
  NotebooksNetworkServiceImplTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    service_ = std::make_unique<TestNotebooksNetworkServiceImpl>(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        identity_test_env_.identity_manager());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestNotebooksNetworkServiceImpl> service_;
};

// Smoke test to ensure methods can be called without crashing.
TEST_F(NotebooksNetworkServiceImplTest, CreateNotebookSmoke) {
  bool callback_called = false;
  service_->CreateNotebook(
      "display_name",
      base::BindLambdaForTesting(
          [&](std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            callback_called = true;
          }));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(
      callback_called);  // Currently does nothing, so callback not called.
}

TEST_F(NotebooksNetworkServiceImplTest, CreateNotebookSourceSmoke) {
  bool callback_called = false;
  service_->CreateNotebookSource(
      "container_id", "item_id",
      base::BindLambdaForTesting(
          [&](std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            callback_called = true;
          }));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(
      callback_called);  // Currently does nothing, so callback not called.
}

// Test that CreateEndpointFetcher constructs the fetcher correctly.
TEST_F(NotebooksNetworkServiceImplTest, CreateEndpointFetcher) {
  GURL test_url("https://example.com");
  std::string test_data = "test_post_data";

  std::unique_ptr<endpoint_fetcher::EndpointFetcher> fetcher =
      service_->CreateEndpointFetcher(test_url, test_data,
                                      TRAFFIC_ANNOTATION_FOR_TESTS);

  ASSERT_TRUE(fetcher);
  EXPECT_EQ(fetcher->GetUrlForTesting(), test_url.spec());
}

}  // namespace

}  // namespace notebooks
