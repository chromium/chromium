// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
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

  base::MockOnceCallback<void(const std::string&)> first_callback;
  EXPECT_CALL(first_callback, Run(expected_dummy_plus_address)).Times(1);

  service.OfferPlusAddressCreation(no_subdomain_origin, first_callback.Get());

  // Repeated calls should invoke the lambda immediately, with the same
  // argument.
  base::MockOnceCallback<void(const std::string&)> second_callback;
  EXPECT_CALL(second_callback, Run(expected_dummy_plus_address)).Times(1);
  service.OfferPlusAddressCreation(subdomain_origin, second_callback.Get());
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
  signin::IdentityTestEnvironment identity_test_env;
  PlusAddressService service(identity_test_env.identity_manager());
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));

  base::MockOnceCallback<void(const std::string&)> callback;
  // Ensure that the lambda wasn't called since there is no signed-in account.
  EXPECT_CALL(callback, Run(testing::_)).Times(0);

  service.OfferPlusAddressCreation(no_subdomain_origin, callback.Get());
}

TEST_F(PlusAddressServiceTest, PlusAddressCreationWithExistingPlus) {
  const std::string expected_dummy_plus_address = "plus+1242@plus.plus";

  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus+plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});

  PlusAddressService service(identity_test_env.identity_manager());
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));

  base::MockOnceCallback<void(const std::string&)> callback;
  // The existing plus should be handled.
  EXPECT_CALL(callback, Run(expected_dummy_plus_address)).Times(1);

  service.OfferPlusAddressCreation(no_subdomain_origin, callback.Get());
}

TEST_F(PlusAddressServiceTest, AbortPlusAddressCreation) {
  const std::string invalid_email = "plus";
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable(invalid_email,
                                         {signin::ConsentLevel::kSignin});

  PlusAddressService service(identity_test_env.identity_manager());
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));

  base::MockOnceCallback<void(const std::string&)> callback;
  // Ensure that the lambda wasn't called since the email address is invalid.
  EXPECT_CALL(callback, Run(testing::_)).Times(0);

  service.OfferPlusAddressCreation(no_subdomain_origin, callback.Get());
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
