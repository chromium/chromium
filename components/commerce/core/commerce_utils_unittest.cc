// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/commerce_utils.h"

#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/test_utils.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "url/gurl.h"

namespace commerce {
namespace {

class CommerceUtilsTest : public testing::Test {};

TEST_F(CommerceUtilsTest, MaybeUseAlternateShoppingServer) {
  auto request_params =
      endpoint_fetcher::EndpointFetcher::RequestParams::Builder(
          endpoint_fetcher::HttpMethod::kGet, TRAFFIC_ANNOTATION_FOR_TESTS);

  // Test with flag OFF.
  commerce::MaybeUseAlternateShoppingServer(request_params);
  const auto flag_off_headers = request_params.Build().headers();
  ASSERT_TRUE(flag_off_headers.empty());

  // Test with flag ON.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(commerce::kShoppingAlternateServer);
  commerce::MaybeUseAlternateShoppingServer(request_params);
  const auto flag_on_headers = request_params.Build().headers();

  ASSERT_EQ(kAlternateServerHeaderName, flag_on_headers[0].key);
  ASSERT_EQ(kAlternateServerHeaderTrueValue, flag_on_headers[0].value);
}

}  // namespace
}  // namespace commerce
