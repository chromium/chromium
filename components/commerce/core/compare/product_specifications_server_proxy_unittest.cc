// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/commerce/core/compare/product_specifications_server_proxy.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
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
            "imageUrl": "http://example.com/image.png",
            "productSpecificationValues": [
              {
                "key": "100000",
                "descriptions": [
                  "Red"
                ]
              }
            ]
          }
        ]
      }
    })";

}  // namespace

class ProductSpecificationsServerProxyTest : public testing::Test {
 protected:
  void SetUp() override {}

  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
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
            ASSERT_EQ("Red",
                      spec->products[0].product_dimension_values[100000][0]);

            looper->Quit();
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace commerce
