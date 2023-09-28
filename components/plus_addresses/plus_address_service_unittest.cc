// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include "base/json/json_writer.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_client.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
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

// Tests the PlusAddressService ability to make network requests.
class PlusAddressServiceRequestsTest : public ::testing::Test {
 public:
  explicit PlusAddressServiceRequestsTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        plus_addresses::kFeature, {{"server-url", server_url.spec()}});
    test_shared_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory);
    plus_profiles_endpoint =
        server_url.Resolve(kServerPlusProfileEndpoint).spec();
  }

 protected:
  std::string MakeCreateResponse(const std::string& facet,
                                 const std::string& plus_address) {
    return base::ReplaceStringPlaceholders(R"({
          "plusProfile":  {
              "facet": "$1",
              "plusEmail": {
                "plusAddress": "$2"
              }
            }
       })",
                                           {facet, plus_address}, nullptr);
  }

  base::test::ScopedFeatureList* features() { return &scoped_feature_list_; }

  GURL server_url = GURL("https://server.example");
  signin::AccessTokenInfo eternal_access_token_info =
      signin::AccessTokenInfo("auth-token", base::Time::Max(), "");
  network::TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory;
  std::string plus_profiles_endpoint;

 private:
  // Required for `IdentityTestEnvironment` and `InProcessDataDecoder` to work.
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder decoder_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PlusAddressServiceRequestsTest, OfferPlusAddressCreation) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});

  PlusAddressClient client(identity_test_env.identity_manager(),
                           test_shared_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_access_token_info);
  PlusAddressService service(identity_test_env.identity_manager(), nullptr,
                             std::move(client));

  base::test::TestFuture<const std::string&> future;
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));
  service.OfferPlusAddressCreation(no_subdomain_origin, future.GetCallback());

  // Check that the future callback is still blocked, and unblock it.
  ASSERT_FALSE(future.IsReady());
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint,
      MakeCreateResponse("test.example", "plus+remote@plus.plus"));
  ASSERT_TRUE(future.IsReady());
  std::string plus_address = future.Get();
  EXPECT_EQ(future.Get(), "plus+remote@plus.plus");

  // Assert that ensuing calls to the same facet do not make a network request.
  const url::Origin subdomain_origin =
      url::Origin::Create(GURL("https://subdomain.test.example"));
  base::test::TestFuture<const std::string&> second_future;
  service.OfferPlusAddressCreation(no_subdomain_origin,
                                   second_future.GetCallback());
  ASSERT_TRUE(second_future.IsReady());
  EXPECT_EQ(second_future.Get(), "plus+remote@plus.plus");
}

// Tests the PlusAddressService ability to make network requests.
class PlusAddressServicePolling : public PlusAddressServiceRequestsTest {
 public:
  PlusAddressServicePolling() {
    features()->Reset();
    features()->InitAndEnableFeatureWithParameters(
        plus_addresses::kFeature, {
                                      {"server-url", server_url.spec()},
                                      {"sync-with-server", "true"},
                                  });
    plus_addresses::RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  struct PlusProfile {
    std::string facet;
    std::string plus_address;
  };

  std::string MakeListResponse(const std::vector<PlusProfile>& profiles) {
    base::Value::Dict response;
    base::Value::List list;
    for (const PlusProfile& profile : profiles) {
      base::Value::Dict profile_dict;
      profile_dict.Set("facet", profile.facet);
      base::Value::Dict plus_email;
      plus_email.Set("plusAddress", profile.plus_address);
      profile_dict.Set("plusEmail", std::move(plus_email));
      list.Append(std::move(profile_dict));
    }
    response.Set("plusProfiles", std::move(list));
    absl::optional<std::string> json = base::WriteJson(response);
    DCHECK(json.has_value());
    return json.value();
  }

  PrefService* prefs() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
};

// TODO (kaklilu): Make this test simulate timer firing instead of directly
// calling SyncPlusAddressMapping.
TEST_F(PlusAddressServicePolling, CallsGetAllPlusAddresses) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  PlusAddressClient client(identity_test_env.identity_manager(),
                           test_shared_loader_factory);
  client.SetAccessTokenInfoForTesting(eternal_access_token_info);
  // The service starts the timer on construction and issues a request to poll.
  PlusAddressService service(identity_test_env.identity_manager(), prefs(),
                             std::move(client));
  // Unblock the initial polling request.
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint, MakeListResponse({}));

  EXPECT_FALSE(service.IsPlusAddress("plus+foo@plus.plus"));
  EXPECT_FALSE(service.IsPlusAddress("plus+bar@plus.plus"));

  service.SyncPlusAddressMapping();
  // Note: The above call blocks until we provide a response to the request.
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint,
      MakeListResponse({PlusProfile{.facet = "foo.com",
                                    .plus_address = "plus+foo@plus.plus"},
                        PlusProfile{.facet = "bar.com",
                                    .plus_address = "plus+bar@plus.plus"}}));

  // The service's mapping should be updated now.
  url::Origin foo_origin = url::Origin::Create(GURL("https://foo.com"));
  ASSERT_TRUE(service.GetPlusAddress(foo_origin).has_value());
  EXPECT_EQ(service.GetPlusAddress(foo_origin).value(), "plus+foo@plus.plus");
  EXPECT_TRUE(service.IsPlusAddress("plus+foo@plus.plus"));

  url::Origin bar_origin = url::Origin::Create(GURL("https://bar.com"));
  ASSERT_TRUE(service.GetPlusAddress(bar_origin).has_value());
  EXPECT_EQ(service.GetPlusAddress(bar_origin).value(), "plus+bar@plus.plus");
  EXPECT_TRUE(service.IsPlusAddress("plus+bar@plus.plus"));
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

TEST_F(PlusAddressServiceEnabledTest, NoIdentityServiceGetEmail) {
  PlusAddressService service;
  EXPECT_EQ(service.GetPrimaryEmail(), absl::nullopt);
}

TEST_F(PlusAddressServiceEnabledTest, SignedOutGetEmail) {
  signin::IdentityTestEnvironment identity_test_env;
  PlusAddressService service(identity_test_env.identity_manager());
  EXPECT_EQ(service.GetPrimaryEmail(), absl::nullopt);
}

TEST_F(PlusAddressServiceEnabledTest, SignedInGetEmail) {
  const std::string expected_email = "plus@plus.plus";
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable(expected_email,
                                         {signin::ConsentLevel::kSignin});
  PlusAddressService service(identity_test_env.identity_manager());
  EXPECT_EQ(service.GetPrimaryEmail(), expected_email);
}
}  // namespace plus_addresses
