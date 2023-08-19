// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/plus_addresses/features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace plus_addresses {

class PlusAddressServiceTest : public ::testing::Test {
 protected:
  // Not used directly, but required for `IdentityTestEnvironment` to work.
  base::test::TaskEnvironment task_environment_;
};

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
  // By default, the `SupportsPlusAddresses` function should return `false`.
  PlusAddressService service;
  EXPECT_FALSE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://test.example"))));
}

TEST_F(PlusAddressServiceTest, OfferPlusAddressCreation) {
  // booleans captured by the lambda to ensure the callbacks are run.
  // See: docs/callback.md;l=352;drc=cc277f0f9a6227eb6f9ef5ee2e5061079fac08c6.
  bool first_called = false;
  bool second_called = false;

  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});

  PlusAddressService service(identity_test_env.identity_manager());

  // The dummy plus address generation function arrives at this string with
  // the eTLD+1 of the origins below. Because that function is temporary, no
  // additional effort is made to ensure it "works".
  const std::string expected_dummy_plus_address = "plus+1242@plus.plus";
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

// Tests for the label overrides. These tests are not in the enabled/disabled
// fixtures as they vary parameters.
TEST_F(PlusAddressServiceTest, LabelOverrides) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Setting the override should result in echoing the override back.
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      plus_addresses::kFeature,
      {{plus_addresses::kEnterprisePlusAddressLabelOverride.name,
        "mattwashere"}});
  PlusAddressService service;
  EXPECT_EQ(service.GetCreateSuggestionLabel(), u"mattwashere");
}

TEST_F(PlusAddressServiceTest, LabelOverrideWithSpaces) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Setting the override should result in echoing the override back.
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      plus_addresses::kFeature,
      {{plus_addresses::kEnterprisePlusAddressLabelOverride.name,
        "matt was here"}});
  PlusAddressService service;
  EXPECT_EQ(service.GetCreateSuggestionLabel(), u"matt was here");
}

TEST_F(PlusAddressServiceTest, NoAccountPlusAddressCreation) {
  bool call_observed = false;

  signin::IdentityTestEnvironment identity_test_env;
  PlusAddressService service(identity_test_env.identity_manager());
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));

  service.OfferPlusAddressCreation(
      no_subdomain_origin,
      base::BindLambdaForTesting(
          [&](const std::string& plus_address) { call_observed = true; }));
  // Ensure that the lambda wasn't called since there is no signed-in account.
  EXPECT_FALSE(call_observed);
}

TEST_F(PlusAddressServiceTest, PlusAddressCreationWithExistingPlus) {
  bool call_observed = false;
  const std::string expected_dummy_plus_address = "plus+1242@plus.plus";

  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus+plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});

  PlusAddressService service(identity_test_env.identity_manager());
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));

  service.OfferPlusAddressCreation(
      no_subdomain_origin,
      base::BindLambdaForTesting([&](const std::string& plus_address) {
        call_observed = true;
        EXPECT_EQ(plus_address, expected_dummy_plus_address);
      }));
  // Ensure that the lambda wasn't called since there is no signed-in account.
  EXPECT_TRUE(call_observed);
}

TEST_F(PlusAddressServiceTest, AbortPlusAddressCreation) {
  bool call_observed = false;

  const std::string invalid_email = "plus";
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable(invalid_email,
                                         {signin::ConsentLevel::kSignin});

  PlusAddressService service(identity_test_env.identity_manager());
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));

  service.OfferPlusAddressCreation(
      no_subdomain_origin,
      base::BindLambdaForTesting(
          [&](const std::string& plus_address) { call_observed = true; }));
  // Ensure that the lambda wasn't called since the email address is invalid.
  EXPECT_FALSE(call_observed);
}

class PlusAddressServiceDisabledTest : public PlusAddressServiceTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndDisableFeature(plus_addresses::kFeature);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PlusAddressServiceDisabledTest, FeatureExplicitlyDisabled) {
  // `SupportsPlusAddresses` should return `false`, even if there's a signed-in
  // user.
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  PlusAddressService service(identity_test_env.identity_manager());
  EXPECT_FALSE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://test.example"))));
}

TEST_F(PlusAddressServiceDisabledTest, DisabledFeatureLabel) {
  // Disabled feature? Show the default generic text.
  PlusAddressService service;
  EXPECT_EQ(service.GetCreateSuggestionLabel(), u"Lorem Ipsum");
}

class PlusAddressServiceEnabledTest : public PlusAddressServiceTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{plus_addresses::kFeature};
};

TEST_F(PlusAddressServiceEnabledTest, NullIdentityManager) {
  // Without an identity manager, the `SupportsPlusAddresses` should return
  // `false`.
  PlusAddressService service;
  EXPECT_FALSE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://test.example"))));
}

TEST_F(PlusAddressServiceEnabledTest, NoSignedInUser) {
  // Without a signed in user, the `SupportsPlusAddresses` function should
  // return `false`.
  signin::IdentityTestEnvironment identity_test_env;
  PlusAddressService service(identity_test_env.identity_manager());
  EXPECT_FALSE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://test.example"))));
}

TEST_F(PlusAddressServiceEnabledTest, FullySupported) {
  // With a signed in user, the `SupportsPlusAddresses` function should return
  // `true`.
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  PlusAddressService service(identity_test_env.identity_manager());
  EXPECT_TRUE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://test.example"))));
}

TEST_F(PlusAddressServiceEnabledTest, DefaultLabel) {
  // Override not set? Show the default generic text.
  PlusAddressService service;
  EXPECT_EQ(service.GetCreateSuggestionLabel(), u"Lorem Ipsum");
}

}  // namespace plus_addresses
