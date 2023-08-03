// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/plus_addresses/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace plus_addresses {

class PlusAddressServiceTest : public ::testing::Test {};

TEST_F(PlusAddressServiceTest, BasicTest) {
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://test.asdf.example"));
  const std::string test_address = "mattwashere";
  PlusAddressService service;
  EXPECT_FALSE(service.IsPlusAddress(test_address));
  service.SavePlusAddress(test_origin, test_address);
  EXPECT_TRUE(service.IsPlusAddress(test_address));
  EXPECT_EQ(service.GetPlusAddress(test_origin), test_address);
  EXPECT_EQ(service.GetPlusAddress(url::Origin()), absl::nullopt);
}

TEST_F(PlusAddressServiceTest, EnsureEtldPlusOneScope) {
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://asdf.example"));
  const url::Origin test_origin_subdomain =
      url::Origin::Create(GURL("https://test.asdf.example"));
  const std::string test_address = "mattwashere";
  PlusAddressService service;
  service.SavePlusAddress(test_origin, test_address);
  EXPECT_EQ(service.GetPlusAddress(test_origin), test_address);
  EXPECT_EQ(service.GetPlusAddress(test_origin_subdomain), test_address);
}

TEST_F(PlusAddressServiceTest, EnsureEtldPlusOneScopeSubdomainAddedFirst) {
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://asdf.example"));
  const url::Origin test_origin_subdomain =
      url::Origin::Create(GURL("https://test.asdf.example"));
  const std::string test_address = "mattwashere";
  PlusAddressService service;
  service.SavePlusAddress(test_origin_subdomain, test_address);
  EXPECT_EQ(service.GetPlusAddress(test_origin), test_address);
  EXPECT_EQ(service.GetPlusAddress(test_origin_subdomain), test_address);
}

TEST_F(PlusAddressServiceTest, DefaultSupportsPlusAddressesState) {
  // Without explicit enablement of the feature, the `SupportsPlusAddresses`
  // function should return `false`.
  PlusAddressService service;
  EXPECT_FALSE(service.SupportsPlusAddresses());
}

TEST_F(PlusAddressServiceTest, FeatureEnabled) {
  // With explicit enablement of the feature, the `SupportsPlusAddresses`
  // function should return `true`.
  base::test::ScopedFeatureList scoped_feature_list;
  // With the flag set, the URL should be filtered.
  scoped_feature_list.InitAndEnableFeature(plus_addresses::kFeature);
  PlusAddressService service;
  EXPECT_TRUE(service.SupportsPlusAddresses());
}

TEST_F(PlusAddressServiceTest, FeatureExplicitlyDisabled) {
  // With explicit disabling of the feature, the `SupportsPlusAddresses`
  // function should return `false`.
  base::test::ScopedFeatureList scoped_feature_list;
  // With the flag set, the URL should be filtered.
  scoped_feature_list.InitAndDisableFeature(plus_addresses::kFeature);
  PlusAddressService service;
  EXPECT_FALSE(service.SupportsPlusAddresses());
}

TEST_F(PlusAddressServiceTest, OfferPlusAddressCreation) {
  PlusAddressService service;
  // booleans captured by the lambda to ensure the callbacks are run.
  // See: docs/callback.md;l=352;drc=cc277f0f9a6227eb6f9ef5ee2e5061079fac08c6.
  bool first_called = false;
  bool second_called = false;

  // The dummy plus address generation function arrives at this string with
  // the eTLD+1 of the origins below. Because that function is temporary, no
  // additional effort is made to ensure it "works".
  const std::string expected_dummy_plus_address = "test+1242@test.example";
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));
  const url::Origin subdomain_origin =
      url::Origin::Create(GURL("https://subdomain.test.example"));

  service.OfferPlusAddressCreation(
      no_subdomain_origin,
      base::BindLambdaForTesting([&](const std::string& plus_address) {
        first_called = true;
        EXPECT_EQ(plus_address, expected_dummy_plus_address);
      }));
  service.OfferPlusAddressCreation(
      subdomain_origin,
      base::BindLambdaForTesting([&](const std::string& plus_address) {
        second_called = true;
        EXPECT_EQ(plus_address, expected_dummy_plus_address);
      }));
  // Ensure that both calls invoked the lambda immediately.
  EXPECT_TRUE(first_called);
  EXPECT_TRUE(second_called);
}

}  // namespace plus_addresses
