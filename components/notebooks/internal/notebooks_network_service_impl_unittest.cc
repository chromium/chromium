// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_network_service_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/notebooks/public/features.h"
#include "components/notebooks/public/notebooks_network_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace notebooks {

namespace {

class FakeEndpointFetcher : public endpoint_fetcher::EndpointFetcher {
 public:
  FakeEndpointFetcher(
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response)
      : endpoint_fetcher::EndpointFetcher(annotation_tag),
        response_(std::move(response)) {}

  ~FakeEndpointFetcher() override = default;

  void Fetch(endpoint_fetcher::EndpointFetcherCallback callback) override {
    std::move(callback).Run(std::move(response_));
  }

 private:
  std::unique_ptr<endpoint_fetcher::EndpointResponse> response_;
};

class TestNotebooksNetworkServiceImpl : public NotebooksNetworkServiceImpl {
 public:
  TestNotebooksNetworkServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager)
      : NotebooksNetworkServiceImpl(std::move(url_loader_factory),
                                    identity_manager) {}

  ~TestNotebooksNetworkServiceImpl() override = default;

  void set_fake_response(
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
    fake_response_ = std::move(response);
  }

  void set_use_real_endpoint_fetcher(bool use_real) {
    use_real_endpoint_fetcher_ = use_real;
  }

  const GURL& last_url() const { return last_url_; }
  const std::string& last_post_data() const { return last_post_data_; }

  // Expose the method for testing.
  using NotebooksNetworkServiceImpl::CreateEndpointFetcher;
  std::unique_ptr<endpoint_fetcher::EndpointFetcher> CreateEndpointFetcher(
      const GURL& url,
      const std::string& post_data,
      const net::NetworkTrafficAnnotationTag& annotation_tag) override {
    last_url_ = url;
    last_post_data_ = post_data;

    if (use_real_endpoint_fetcher_) {
      return NotebooksNetworkServiceImpl::CreateEndpointFetcher(url, post_data,
                                                                annotation_tag);
    }

    return std::make_unique<FakeEndpointFetcher>(annotation_tag,
                                                 std::move(fake_response_));
  }

  using NotebooksNetworkServiceImpl::ConstructServiceURL;

 private:
  bool use_real_endpoint_fetcher_ = false;
  GURL last_url_;
  std::string last_post_data_;
  std::unique_ptr<endpoint_fetcher::EndpointResponse> fake_response_;
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

  void EnableNotebooksFeature(const std::string& base_url,
                              const std::string& source_suffix,
                              const std::string& product_id) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kNotebooks, {{"notebooks_api_base_url", base_url},
                               {"notebook_source_url_suffix", source_suffix},
                               {"provenance_origin_product_id", product_id}});
  }

  std::unique_ptr<endpoint_fetcher::EndpointResponse> CreateResponse(
      int http_status_code,
      const std::string& response_body) {
    auto response = std::make_unique<endpoint_fetcher::EndpointResponse>();
    response->http_status_code = http_status_code;
    response->response = response_body;
    return response;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<TestNotebooksNetworkServiceImpl> service_;
};

// Test that CreateEndpointFetcher constructs the fetcher correctly.
TEST_F(NotebooksNetworkServiceImplTest, CreateEndpointFetcher) {
  GURL test_url("https://example.com");
  service_->set_use_real_endpoint_fetcher(true);
  std::string test_data = "test_post_data";

  std::unique_ptr<endpoint_fetcher::EndpointFetcher> fetcher =
      service_->CreateEndpointFetcher(test_url, test_data,
                                      TRAFFIC_ANNOTATION_FOR_TESTS);

  ASSERT_TRUE(fetcher);
  EXPECT_EQ(fetcher->GetUrlForTesting(), test_url.spec());
}

TEST_F(NotebooksNetworkServiceImplTest, ConstructServiceURL_NoPathSuccess) {
  std::string base_url = "https://example.com/v1/notebooks";
  std::string product_id = "chrome";
  EnableNotebooksFeature(base_url, "sources", product_id);

  GURL constructed_url = service_->ConstructServiceURL("");

  EXPECT_TRUE(constructed_url.spec().starts_with(base_url));
  std::string actual_product_id;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      constructed_url, "provenance.origin_product_type", &actual_product_id));
  EXPECT_EQ(actual_product_id, product_id);
}

TEST_F(NotebooksNetworkServiceImplTest, ConstructServiceURL_FeatureNotEnabled) {
  GURL constructed_url = service_->ConstructServiceURL("");

  EXPECT_EQ(constructed_url, GURL());
}

TEST_F(NotebooksNetworkServiceImplTest, CreateNotebook_InvalidURL) {
  EnableNotebooksFeature("://invalid", "sources", "chrome");

  base::RunLoop run_loop;
  service_->CreateNotebook(
      "test_notebook",
      base::BindLambdaForTesting(
          [&](std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            EXPECT_FALSE(result);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(NotebooksNetworkServiceImplTest, CreateNotebook_FeatureNotEnabled) {
  base::RunLoop run_loop;
  service_->CreateNotebook(
      "test_notebook",
      base::BindLambdaForTesting(
          [&](std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            EXPECT_FALSE(result);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(NotebooksNetworkServiceImplTest, CreateNotebook_Success) {
  std::string product_id = "chrome";
  std::string notebook_display_name = "My notebook";
  EnableNotebooksFeature("https://example.com/v1/notebooks", "sources",
                         product_id);
  service_->set_fake_response(CreateResponse(net::HTTP_OK, "response_body"));

  base::RunLoop run_loop;
  service_->CreateNotebook(
      notebook_display_name,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            ASSERT_TRUE(result);
            EXPECT_EQ(result->status,
                      NotebooksNetworkService::NetworkLoaderStatus::kSuccess);
            EXPECT_EQ(result->network_error_code, net::HTTP_OK);
            EXPECT_EQ(result->result_bytes, "response_body");
            run_loop.Quit();
          }));
  run_loop.Run();

  GURL expected_url = service_->ConstructServiceURL("");
  GURL actual_url = service_->last_url();
  EXPECT_EQ(expected_url, actual_url);

  std::optional<base::DictValue> dict = base::JSONReader::ReadDict(
      service_->last_post_data(), base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict.has_value());
  const std::string* display_name = dict->FindString("display_name");
  ASSERT_TRUE(display_name);
  EXPECT_EQ(*display_name, notebook_display_name);
}

TEST_F(NotebooksNetworkServiceImplTest, CreateNotebook_TransientFailure) {
  std::string server_error = "server_error";
  EnableNotebooksFeature("https://example.com/v1/notebooks", "sources",
                         "chrome");
  service_->set_fake_response(
      CreateResponse(net::HTTP_INTERNAL_SERVER_ERROR, server_error));

  base::RunLoop run_loop;
  service_->CreateNotebook(
      "My Notebook",
      base::BindLambdaForTesting(
          [&, server_error](
              std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            ASSERT_TRUE(result);
            EXPECT_EQ(result->status,
                      NotebooksNetworkService::NetworkLoaderStatus::
                          kTransientFailure);
            EXPECT_EQ(result->network_error_code,
                      net::HTTP_INTERNAL_SERVER_ERROR);
            EXPECT_EQ(result->result_bytes, server_error);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(NotebooksNetworkServiceImplTest, CreateNotebook_PersistentFailure) {
  std::string bad_request_error = "bad_request";
  EnableNotebooksFeature("https://example.com/v1/notebooks", "sources",
                         "chrome");
  service_->set_fake_response(
      CreateResponse(net::HTTP_BAD_REQUEST, bad_request_error));

  base::RunLoop run_loop;
  service_->CreateNotebook(
      "My Notebook",
      base::BindLambdaForTesting(
          [&, bad_request_error](
              std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            ASSERT_TRUE(result);
            EXPECT_EQ(result->status,
                      NotebooksNetworkService::NetworkLoaderStatus::
                          kPersistentFailure);
            EXPECT_EQ(result->network_error_code, net::HTTP_BAD_REQUEST);
            EXPECT_EQ(result->result_bytes, bad_request_error);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(NotebooksNetworkServiceImplTest, CreateNotebook_NullResponse) {
  EnableNotebooksFeature("https://example.com/v1/notebooks", "sources",
                         "chrome");
  service_->set_fake_response(nullptr);

  bool callback_called = false;
  service_->CreateNotebook(
      "My Notebook",
      base::BindLambdaForTesting(
          [&callback_called](
              std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            callback_called = true;
            EXPECT_FALSE(result);
          }));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_called);
}

TEST_F(NotebooksNetworkServiceImplTest, CreateNotebookSource_Success) {
  std::string url_source_suffix = "sources";
  std::string url_path_prefix = "/v1/notebooks";
  std::string base_url = base::StrCat({"https://example.com", url_path_prefix});
  std::string source_result = "source_result";
  net::HttpStatusCode status_code = net::HTTP_OK;
  EnableNotebooksFeature(base_url, url_source_suffix, "chrome");
  service_->set_fake_response(CreateResponse(status_code, source_result));

  base::RunLoop run_loop;
  std::string notebook_id = "1234";
  std::string source_id = "5678";
  service_->CreateNotebookSource(
      notebook_id, source_id,
      base::BindLambdaForTesting(
          [&](std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            ASSERT_TRUE(result);
            EXPECT_EQ(result->status,
                      NotebooksNetworkService::NetworkLoaderStatus::kSuccess);
            EXPECT_EQ(result->network_error_code, status_code);
            EXPECT_EQ(result->result_bytes, source_result);
            run_loop.Quit();
          }));
  run_loop.Run();

  std::string expected_url_path =
      base::StrCat({url_path_prefix, "/", notebook_id, "/", url_source_suffix,
                    "/", source_id});
  EXPECT_EQ(service_->last_url().path(), expected_url_path);

  std::optional<base::DictValue> dict = base::JSONReader::ReadDict(
      service_->last_post_data(), base::JSON_PARSE_RFC);
  ASSERT_TRUE(dict.has_value());
  const base::DictValue* external_id = dict->FindDict("external_identifier");
  ASSERT_TRUE(external_id);
  const std::string* id = external_id->FindString("id");
  ASSERT_TRUE(id);
  EXPECT_EQ(*id, source_id);
}

TEST_F(NotebooksNetworkServiceImplTest, CreateNotebookSource_EscapesIds) {
  std::string url_source_suffix = "sources";

  EnableNotebooksFeature("https://example.com", url_source_suffix, "chrome");
  service_->set_fake_response(CreateResponse(net::HTTP_OK, "ok"));

  base::RunLoop run_loop;
  service_->CreateNotebookSource(
      "notebooks/123", "tabs?456",
      base::BindLambdaForTesting(
          [&run_loop](
              std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_EQ(service_->last_url().path(),
            base::StrCat({"/", "notebooks%2F123", "/", url_source_suffix, "/",
                          "tabs%3F456"}));
}

TEST_F(NotebooksNetworkServiceImplTest, CreateNotebookSource_NoSourceSuffix) {
  EnableNotebooksFeature("https://validbaseurl.com", "sources", "chrome");

  base::RunLoop run_loop;
  service_->CreateNotebookSource(
      "", "",
      base::BindLambdaForTesting(
          [&](std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            EXPECT_FALSE(result);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(NotebooksNetworkServiceImplTest, CreateNotebookSource_EmptyIdArguments) {
  EnableNotebooksFeature("https://validbaseurl.com", "", "chrome");

  base::RunLoop run_loop;
  service_->CreateNotebookSource(
      "test_notebook_id", "test_source_id",
      base::BindLambdaForTesting(
          [&](std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            EXPECT_FALSE(result);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(NotebooksNetworkServiceImplTest, CreateNotebookSource_InvalidURL) {
  EnableNotebooksFeature("://invalid", "sources", "chrome");

  base::RunLoop run_loop;
  service_->CreateNotebookSource(
      "test_notebook_id", "test_source_id",
      base::BindLambdaForTesting(
          [&](std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            EXPECT_FALSE(result);
            run_loop.Quit();
          }));
  run_loop.Run();
}

TEST_F(NotebooksNetworkServiceImplTest,
       CreateNotebookSource_FeatureNotEnabled) {
  base::RunLoop run_loop;
  service_->CreateNotebookSource(
      "test_notebook_id", "test_source_id",
      base::BindLambdaForTesting(
          [&](std::unique_ptr<NotebooksNetworkService::LoadResult> result) {
            EXPECT_FALSE(result);
            run_loop.Quit();
          }));
  run_loop.Run();
}

}  // namespace

}  // namespace notebooks
