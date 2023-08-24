// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_client.h"

#include "base/test/scoped_feature_list.h"
#include "components/plus_addresses/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace plus_addresses {
TEST(PlusAddressClientTest, ChecksUrlParamIsValidGurl) {
  std::string server_url = "https://foo.com/";
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      kFeature, {{kEnterprisePlusAddressServerUrl.name, server_url}});
  PlusAddressClient client;
  ASSERT_TRUE(client.GetServerUrlForTesting().has_value());
  EXPECT_EQ(client.GetServerUrlForTesting().value(), server_url);
}

TEST(PlusAddressClientTest, RejectsNonUrlStrings) {
  base::test::ScopedFeatureList feature;
  feature.InitAndEnableFeatureWithParameters(
      kFeature, {{kEnterprisePlusAddressServerUrl.name, "kirubeldotcom"}});
  PlusAddressClient client;
  EXPECT_FALSE(client.GetServerUrlForTesting().has_value());
}

}  // namespace plus_addresses
