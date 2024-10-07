// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/compare/product_specifications_server_proxy.h"

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_account_checker.h"
#include "components/commerce/core/pref_names.h"
#include "components/commerce/core/test_utils.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/endpoint_fetcher/mock_endpoint_fetcher.h"
#include "components/prefs/testing_pref_service.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace commerce {
namespace {

const std::string kSimpleResponse = R"(
    {
      "productSpecifications": {
        "productSpecificationSections": [
          {
            "key": "100000",
            "title": "Color"
          }
        ],
        "productSpecifications": [
          {
            "identifiers": {
              "gpcId": "12345",
              "mid": "/g/abcd"
            },
            "title": "Circle",
            "summaryDescription": [
              {
                "text": "Circle is round",
                "urls": [
                  {
                    "url": "http://example.com/circle/",
                    "title": "Circles",
                    "faviconUrl": "http://example.com/favicon.png",
                    "thumbnailImageUrl": "http://example.com/thumbnail.png"
                  }
                ]
              }
            ],
            "imageUrl": "http://example.com/image.png",
            "buyingOptionsUrl": "http://example.com/jackpot",
            "productSpecificationValues": [
              {
                "key": "100000",
                "specificationDescriptions": [
                  {
                    "options": [
                      {
                        "description": [
                          {
                            "text": "Red",
                            "urls": [
                              {"url": "http://example.com/red/"}
                            ]
                          }
                        ]
                      }
                    ],
                    "label": "Color",
                    "alternativeText": "The circle color",
                    "attributeId": "0"
                  }
                ],
                "summaryDescription": [
                  {
                    "text": "Descriptions summary",
                    "urls": [
                      {"url": "http://example.com/descriptions/"}
                    ]
                  }
                ]
              }
            ]
          }
        ]
      }
    })";

class MockProductSpecificationsServerProxy
    : public ProductSpecificationsServerProxy {
 public:
  explicit MockProductSpecificationsServerProxy(AccountChecker* account_checker)
      : ProductSpecificationsServerProxy(account_checker, nullptr, nullptr) {}
  MockProductSpecificationsServerProxy(
      const MockProductSpecificationsServerProxy&) = delete;
  MockProductSpecificationsServerProxy operator=(
      const MockProductSpecificationsServerProxy&) = delete;
  ~MockProductSpecificationsServerProxy() override = default;
  MOCK_METHOD(std::unique_ptr<EndpointFetcher>,
              CreateEndpointFetcher,
              (const GURL& url,
               const std::string& http_method,
               const std::string& post_data),
              (override));
};

}  // namespace

class ProductSpecificationsServerProxyTest : public testing::Test {
 public:
  ProductSpecificationsServerProxyTest()
      : prefs_(std::make_unique<TestingPrefServiceSimple>()) {}

 protected:
  void SetUp() override {
    account_checker_ = std::make_unique<MockAccountChecker>();
    account_checker_->SetCountry("us");
    account_checker_->SetLocale("en-us");
    account_checker_->SetSignedIn(true);
    account_checker_->SetAnonymizedUrlDataCollectionEnabled(true);
    ON_CALL(*account_checker_, IsSyncTypeEnabled)
        .WillByDefault(testing::Return(true));

    RegisterCommercePrefs(prefs_->registry());
    SetTabCompareEnterprisePolicyPref(prefs_.get(), 0);
    account_checker_->SetPrefs(prefs_.get());

    server_proxy_ = std::make_unique<MockProductSpecificationsServerProxy>(
        account_checker_.get());
  }

  void TearDown() override { test_features_.Reset(); }

  std::unique_ptr<TestingPrefServiceSimple> prefs_;
  std::unique_ptr<MockAccountChecker> account_checker_;
  std::unique_ptr<MockProductSpecificationsServerProxy> server_proxy_;

  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

 protected:
  base::test::ScopedFeatureList test_features_;
};

TEST_F(ProductSpecificationsServerProxyTest, JsonToProductSpecifications) {
  base::RunLoop run_loop;
  data_decoder::DataDecoder::ParseJsonIsolated(
      kSimpleResponse,
      base::BindOnce(
          [](base::RunLoop* looper,
             data_decoder::DataDecoder::ValueOrError result) {
            ASSERT_TRUE(result.has_value());

            std::optional<ProductSpecifications> spec =
                ProductSpecificationsServerProxy::
                    ProductSpecificationsFromJsonResponse(result.value());

            ASSERT_TRUE(spec.has_value());

            ASSERT_EQ(1u, spec->product_dimension_map.size());
            ASSERT_EQ("Color", spec->product_dimension_map[100000]);

            ASSERT_EQ(1u, spec->products.size());
            ASSERT_EQ(12345u, spec->products[0].product_cluster_id);
            ASSERT_EQ("/g/abcd", spec->products[0].mid);
            ASSERT_EQ("Circle", spec->products[0].title);
            ASSERT_EQ("http://example.com/image.png",
                      spec->products[0].image_url.spec());
            ASSERT_EQ("http://example.com/jackpot",
                      spec->products[0].buying_options_url.spec());
            ASSERT_EQ("Circle is round", spec->products[0].summary[0].text);
            ASSERT_EQ("http://example.com/circle/",
                      spec->products[0].summary[0].urls[0].url.spec());
            ASSERT_EQ("http://example.com/favicon.png", spec->products[0]
                                                            .summary[0]
                                                            .urls[0]
                                                            .favicon_url.value()
                                                            .spec());
            ASSERT_EQ("http://example.com/thumbnail.png",
                      spec->products[0]
                          .summary[0]
                          .urls[0]
                          .thumbnail_url.value()
                          .spec());
            ASSERT_EQ(u"Circles", spec->products[0].summary[0].urls[0].title);

            const ProductSpecifications::Description& color_desc =
                spec->products[0]
                    .product_dimension_values[100000]
                    .descriptions[0];
            ASSERT_EQ("Color", color_desc.label);
            ASSERT_EQ("The circle color", color_desc.alt_text);
            ASSERT_EQ("Red", color_desc.options[0].descriptions[0].text);
            ASSERT_EQ("http://example.com/red/",
                      color_desc.options[0].descriptions[0].urls[0].url.spec());

            looper->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ProductSpecificationsServerProxyTest,
       GetProductSpecificationsForClusterIds) {
  test_features_.InitWithFeatures({commerce::kProductSpecifications}, {});
  base::RunLoop run_loop;

  ON_CALL(*server_proxy_, CreateEndpointFetcher).WillByDefault([]() {
    std::unique_ptr<MockEndpointFetcher> fetcher =
        std::make_unique<MockEndpointFetcher>();
    fetcher->SetFetchResponse(kSimpleResponse);
    return fetcher;
  });

  server_proxy_->GetProductSpecificationsForClusterIds(
      {12345}, base::BindOnce(
                   [](base::RunLoop* looper, std::vector<uint64_t> ids,
                      std::optional<ProductSpecifications> spec) {
                     ASSERT_TRUE(spec.has_value());

                     ASSERT_EQ(12345u, ids[0]);

                     ASSERT_EQ(1u, spec->product_dimension_map.size());
                     ASSERT_EQ("Color", spec->product_dimension_map[100000]);

                     ASSERT_EQ(1u, spec->products.size());
                     ASSERT_EQ(12345u, spec->products[0].product_cluster_id);
                     ASSERT_EQ("/g/abcd", spec->products[0].mid);
                     ASSERT_EQ("Circle", spec->products[0].title);
                     ASSERT_EQ("http://example.com/image.png",
                               spec->products[0].image_url.spec());
                     ASSERT_EQ("http://example.com/jackpot",
                               spec->products[0].buying_options_url.spec());
                     ASSERT_EQ("Circle is round",
                               spec->products[0].summary[0].text);
                     ASSERT_EQ("http://example.com/circle/",
                               spec->products[0].summary[0].urls[0].url.spec());

                     const ProductSpecifications::Description& color_desc =
                         spec->products[0]
                             .product_dimension_values[100000]
                             .descriptions[0];
                     ASSERT_EQ("Color", color_desc.label);
                     ASSERT_EQ("The circle color", color_desc.alt_text);
                     ASSERT_EQ("Red",
                               color_desc.options[0].descriptions[0].text);
                     ASSERT_EQ("http://example.com/red/", color_desc.options[0]
                                                              .descriptions[0]
                                                              .urls[0]
                                                              .url.spec());

                     looper->Quit();
                   },
                   &run_loop));

  run_loop.Run();
}

TEST_F(ProductSpecificationsServerProxyTest,
       GetProductSpecificationsForClusterIds_ApiDisabled) {
  base::RunLoop run_loop;

  ON_CALL(*server_proxy_, CreateEndpointFetcher).WillByDefault([]() {
    std::unique_ptr<MockEndpointFetcher> fetcher =
        std::make_unique<MockEndpointFetcher>();
    fetcher->SetFetchResponse(kSimpleResponse);
    return fetcher;
  });
  server_proxy_->GetProductSpecificationsForClusterIds(
      {12345}, base::BindOnce(
                   [](base::RunLoop* looper, std::vector<uint64_t> ids,
                      std::optional<ProductSpecifications> spec) {
                     ASSERT_FALSE(spec.has_value());

                     looper->Quit();
                   },
                   &run_loop));

  run_loop.Run();
}

TEST_F(ProductSpecificationsServerProxyTest,
       GetProductSpecificationsForClusterIds_BadJson) {
  base::RunLoop run_loop;

  ON_CALL(*server_proxy_, CreateEndpointFetcher).WillByDefault([]() {
    std::unique_ptr<MockEndpointFetcher> fetcher =
        std::make_unique<MockEndpointFetcher>();
    fetcher->SetFetchResponse("_Malformed JSON_");
    return fetcher;
  });

  server_proxy_->GetProductSpecificationsForClusterIds(
      {12345}, base::BindOnce(
                   [](base::RunLoop* looper, std::vector<uint64_t> ids,
                      std::optional<ProductSpecifications> spec) {
                     ASSERT_FALSE(spec.has_value());

                     looper->Quit();
                   },
                   &run_loop));

  run_loop.Run();
}

TEST_F(ProductSpecificationsServerProxyTest,
       GetProductSpecificationsForClusterIds_BadNetwork) {
  base::RunLoop run_loop;

  ON_CALL(*server_proxy_, CreateEndpointFetcher).WillByDefault([]() {
    std::unique_ptr<MockEndpointFetcher> fetcher =
        std::make_unique<MockEndpointFetcher>();
    fetcher->SetFetchResponse(kSimpleResponse, net::ERR_ACCESS_DENIED);
    return fetcher;
  });

  server_proxy_->GetProductSpecificationsForClusterIds(
      {12345}, base::BindOnce(
                   [](base::RunLoop* looper, std::vector<uint64_t> ids,
                      std::optional<ProductSpecifications> spec) {
                     ASSERT_FALSE(spec.has_value());

                     looper->Quit();
                   },
                   &run_loop));

  run_loop.Run();
}

}  // namespace commerce
