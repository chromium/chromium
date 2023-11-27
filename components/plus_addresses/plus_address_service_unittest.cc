// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_client.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_test_utils.h"
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
  EXPECT_EQ(service.GetPlusProfile(test_origin)->plus_address, test_address);
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
  EXPECT_EQ(service.GetPlusProfile(test_origin)->plus_address, test_address);
  EXPECT_EQ(service.GetPlusProfile(test_origin_subdomain)->plus_address,
            test_address);
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
  EXPECT_EQ(service.GetPlusProfile(test_origin)->plus_address, test_address);
  EXPECT_EQ(service.GetPlusProfile(test_origin_subdomain)->plus_address,
            test_address);
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

  base::MockOnceCallback<void(const std::string&)> offer_callback;
  base::MockOnceCallback<void(const PlusProfileOrError&)> reserve_callback;
  base::MockOnceCallback<void(const PlusProfileOrError&)> confirm_callback;
  // Ensure that the lambdas aren't called since there is no signed-in account.
  EXPECT_CALL(offer_callback, Run).Times(0);
  service.OfferPlusAddressCreation(no_subdomain_origin, offer_callback.Get());
  EXPECT_CALL(reserve_callback, Run).Times(0);
  service.ReservePlusAddress(no_subdomain_origin, reserve_callback.Get());
  EXPECT_CALL(confirm_callback, Run).Times(0);
  service.ConfirmPlusAddress(no_subdomain_origin, "unused",
                             confirm_callback.Get());
}

TEST_F(PlusAddressServiceTest, AbortPlusAddressCreation) {
  const std::string invalid_email = "plus";
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable(invalid_email,
                                         {signin::ConsentLevel::kSignin});

  PlusAddressService service(identity_test_env.identity_manager());
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));

  base::MockOnceCallback<void(const std::string&)> offer_callback;
  base::MockOnceCallback<void(const PlusProfileOrError&)> reserve_callback;
  base::MockOnceCallback<void(const PlusProfileOrError&)> confirm_callback;
  // Ensure that the lambdas aren't called since there is no signed-in account.
  EXPECT_CALL(offer_callback, Run).Times(0);
  service.OfferPlusAddressCreation(no_subdomain_origin, offer_callback.Get());
  EXPECT_CALL(reserve_callback, Run).Times(0);
  service.ReservePlusAddress(no_subdomain_origin, reserve_callback.Get());
  EXPECT_CALL(confirm_callback, Run).Times(0);
  service.ConfirmPlusAddress(no_subdomain_origin, "unused",
                             confirm_callback.Get());
}

// Tests the PlusAddressService ability to make network requests.
class PlusAddressServiceRequestsTest : public ::testing::Test {
 public:
  explicit PlusAddressServiceRequestsTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        plus_addresses::kFeature,
        {{"server-url", server_url.spec()}, {"oauth-scope", "scope.example"}});
    test_shared_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory);
    plus_profiles_endpoint =
        server_url.Resolve(kServerPlusProfileEndpoint).spec();
    reserve_plus_address_endpoint =
        server_url.Resolve(kServerReservePlusAddressEndpoint).spec();
    confirm_plus_address_endpoint =
        server_url.Resolve(kServerCreatePlusAddressEndpoint).spec();
  }

 protected:
  base::test::ScopedFeatureList* features() { return &scoped_feature_list_; }

  GURL server_url = GURL("https://server.example");
  signin::AccessTokenInfo eternal_access_token_info =
      signin::AccessTokenInfo("auth-token", base::Time::Max(), "");
  network::TestURLLoaderFactory test_url_loader_factory;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory;
  std::string plus_profiles_endpoint;
  std::string reserve_plus_address_endpoint;
  std::string confirm_plus_address_endpoint;

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
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressService service(
      identity_test_env.identity_manager(), nullptr,
      PlusAddressClient(identity_test_env.identity_manager(),
                        test_shared_loader_factory));

  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));
  const std::string site = "test.example";
  const std::string plus_address = "plus+remote@plus.plus";

  base::test::TestFuture<const std::string&> future;
  service.OfferPlusAddressCreation(no_subdomain_origin, future.GetCallback());

  // Check that the future callback is blocked and unblock it.
  ASSERT_FALSE(future.IsReady());
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint,
      test::MakeCreationResponse(
          PlusProfile({.facet = site, .plus_address = plus_address})));
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), plus_address);

  // Assert that ensuing calls to the same facet do not make a network request.
  const url::Origin subdomain_origin =
      url::Origin::Create(GURL("https://subdomain.test.example"));
  base::test::TestFuture<const std::string&> second_future;
  service.OfferPlusAddressCreation(no_subdomain_origin,
                                   second_future.GetCallback());
  ASSERT_TRUE(second_future.IsReady());
  EXPECT_EQ(second_future.Get(), plus_address);
}

TEST_F(PlusAddressServiceRequestsTest, ReservePlusAddress_ReturnsUnconfirmed) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressService service(
      identity_test_env.identity_manager(), nullptr,
      PlusAddressClient(identity_test_env.identity_manager(),
                        test_shared_loader_factory));

  base::test::TestFuture<const PlusProfileOrError&> future;
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));
  std::string site = "test.example";
  std::string plus_address = "plus+remote@plus.plus";

  service.ReservePlusAddress(no_subdomain_origin, future.GetCallback());

  // Check that the future callback is still blocked, and unblock it.
  ASSERT_FALSE(future.IsReady());
  test_url_loader_factory.SimulateResponseForPendingRequest(
      reserve_plus_address_endpoint,
      test::MakeCreationResponse(PlusProfile({.facet = site,
                                              .plus_address = plus_address,
                                              .is_confirmed = false})));
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get()->plus_address, plus_address);

  // The service should not save plus_address if it hasn't been confirmed yet.
  EXPECT_FALSE(service.IsPlusAddress(plus_address));
}

TEST_F(PlusAddressServiceRequestsTest, ReservePlusAddress_ReturnsConfirmed) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressService service(
      identity_test_env.identity_manager(), nullptr,
      PlusAddressClient(identity_test_env.identity_manager(),
                        test_shared_loader_factory));

  base::test::TestFuture<const PlusProfileOrError&> future;
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));
  const std::string site = "test.example";
  const std::string plus_address = "plus+remote@plus.plus";

  service.ReservePlusAddress(no_subdomain_origin, future.GetCallback());

  // Check that the future callback is still blocked, and unblock it.
  ASSERT_FALSE(future.IsReady());
  test_url_loader_factory.SimulateResponseForPendingRequest(
      reserve_plus_address_endpoint,
      test::MakeCreationResponse(PlusProfile({.facet = site,
                                              .plus_address = plus_address,
                                              .is_confirmed = true})));
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get()->plus_address, plus_address);

  // The service should save plus_address if it has already been confirmed.
  EXPECT_TRUE(service.IsPlusAddress(plus_address));
}

TEST_F(PlusAddressServiceRequestsTest, ReservePlusAddress_Fails) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressService service(
      identity_test_env.identity_manager(), nullptr,
      PlusAddressClient(identity_test_env.identity_manager(),
                        test_shared_loader_factory));

  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));
  const std::string site = "test.example";
  const std::string plus_address = "plus+remote@plus.plus";

  base::test::TestFuture<const PlusProfileOrError&> future;
  service.ReservePlusAddress(no_subdomain_origin, future.GetCallback());

  // Check that the future callback is still blocked, and unblock it.
  ASSERT_FALSE(future.IsReady());
  test_url_loader_factory.SimulateResponseForPendingRequest(
      reserve_plus_address_endpoint, "", net::HTTP_BAD_REQUEST);
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(PlusAddressServiceRequestsTest, ConfirmPlusAddress_Successful) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressService service(
      identity_test_env.identity_manager(), nullptr,
      PlusAddressClient(identity_test_env.identity_manager(),
                        test_shared_loader_factory));

  base::test::TestFuture<const PlusProfileOrError&> future;
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));
  const std::string site = "test.example";
  const std::string plus_address = "plus+remote@plus.plus";

  service.ConfirmPlusAddress(no_subdomain_origin, plus_address,
                             future.GetCallback());

  // Check that the future callback is still blocked, and unblock it.
  ASSERT_FALSE(future.IsReady());
  test_url_loader_factory.SimulateResponseForPendingRequest(
      confirm_plus_address_endpoint,
      test::MakeCreationResponse(
          PlusProfile({.facet = site, .plus_address = plus_address})));
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get()->plus_address, plus_address);
  // Verify that the plus_address is saved when confirmation is successful.
  EXPECT_TRUE(service.IsPlusAddress(plus_address));

  // Assert that ensuing calls to the same facet do not make a network request.
  const url::Origin subdomain_origin =
      url::Origin::Create(GURL("https://subdomain.test.example"));
  base::test::TestFuture<const PlusProfileOrError&> second_future;
  service.ConfirmPlusAddress(no_subdomain_origin, plus_address,
                             second_future.GetCallback());
  ASSERT_TRUE(second_future.IsReady());
  EXPECT_EQ(second_future.Get()->plus_address, plus_address);
}

TEST_F(PlusAddressServiceRequestsTest, ConfirmPlusAddress_Fails) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressService service(
      identity_test_env.identity_manager(), nullptr,
      PlusAddressClient(identity_test_env.identity_manager(),
                        test_shared_loader_factory));
  std::string plus_address = "plus+remote@plus.plus";
  ASSERT_FALSE(service.IsPlusAddress(plus_address));

  base::test::TestFuture<const PlusProfileOrError&> future;
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));
  service.ConfirmPlusAddress(no_subdomain_origin, plus_address,
                             future.GetCallback());

  // Check that the future callback is still blocked, and unblock it.
  ASSERT_FALSE(future.IsReady());
  test_url_loader_factory.SimulateResponseForPendingRequest(
      confirm_plus_address_endpoint, "", net::HTTP_BAD_REQUEST);
  ASSERT_TRUE(future.IsReady());

  // An error is propagated from the callback and plus_address is not saved.
  EXPECT_FALSE(future.Get().has_value());
  EXPECT_FALSE(service.IsPlusAddress(plus_address));
}

// Doesn't run on ChromeOS since ClearPrimaryAccount() doesn't exist for it.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PlusAddressServiceRequestsTest,
       PrimaryAccountCleared_TogglesPlusAddressCreationOff) {
  // Setup state where the PlusAddressService would create a PlusAddress.
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakePrimaryAccountAvailable("plus@plus.plus",
                                                signin::ConsentLevel::kSignin);
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressService service(
      identity_test_env.identity_manager(), nullptr,
      PlusAddressClient(identity_test_env.identity_manager(),
                        test_shared_loader_factory));
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://test.example"));
  const std::string site = "test.example";
  const std::string plus_address = "plus+remote@plus.plus";
  // Toggle creation off by removing the primary account.
  identity_test_env.ClearPrimaryAccount();

  // Verify that Plus Address creation doesn't occur.
  service.ReservePlusAddress(test_origin, base::DoNothing());
  service.ConfirmPlusAddress(test_origin, plus_address, base::DoNothing());
  EXPECT_EQ(test_url_loader_factory.NumPending(), 0);

  // Toggle creation back on by signing in again.
  identity_test_env.MakePrimaryAccountAvailable("plus@plus.plus",
                                                signin::ConsentLevel::kSignin);

  // Verify that Plus Address creation occurs and makes a network request.
  base::test::TestFuture<const PlusProfileOrError&> reserve;
  service.ReservePlusAddress(test_origin, reserve.GetCallback());
  EXPECT_EQ(test_url_loader_factory.NumPending(), 1);
  test_url_loader_factory.SimulateResponseForPendingRequest(
      reserve_plus_address_endpoint,
      test::MakeCreationResponse(
          PlusProfile({.facet = site, .plus_address = plus_address})));
  EXPECT_EQ(reserve.Get()->plus_address, plus_address);

  base::test::TestFuture<const PlusProfileOrError&> confirm;
  service.ConfirmPlusAddress(test_origin, plus_address, confirm.GetCallback());
  EXPECT_EQ(test_url_loader_factory.NumPending(), 1);
  test_url_loader_factory.SimulateResponseForPendingRequest(
      confirm_plus_address_endpoint,
      test::MakeCreationResponse(
          PlusProfile({.facet = site, .plus_address = plus_address})));
  EXPECT_EQ(confirm.Get()->plus_address, plus_address);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PlusAddressServiceRequestsTest,
       PrimaryRefreshTokenError_TogglesPlusAddressCreationOff) {
  // Setup state where the PlusAddressService would create a PlusAddress.
  signin::IdentityTestEnvironment identity_test_env;
  AccountInfo primary_account = identity_test_env.MakePrimaryAccountAvailable(
      "plus@plus.plus", signin::ConsentLevel::kSignin);
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressService service(
      identity_test_env.identity_manager(), nullptr,
      PlusAddressClient(identity_test_env.identity_manager(),
                        test_shared_loader_factory));
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://test.example"));
  const std::string site = "test.example";
  const std::string plus_address = "plus+remote@plus.plus";

  // Toggle creation off by triggering an error for the primary refresh token.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Verify that Plus Address creation doesn't occur.
  service.ReservePlusAddress(test_origin, base::DoNothing());
  service.ConfirmPlusAddress(test_origin, plus_address, base::DoNothing());
  EXPECT_EQ(test_url_loader_factory.NumPending(), 0);

  // Toggle creation back on by removing the error.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));

  // Verify that Plus Address creation occurs and makes a network request.
  base::test::TestFuture<const PlusProfileOrError&> reserve;
  service.ReservePlusAddress(test_origin, reserve.GetCallback());
  EXPECT_EQ(test_url_loader_factory.NumPending(), 1);
  test_url_loader_factory.SimulateResponseForPendingRequest(
      reserve_plus_address_endpoint,
      test::MakeCreationResponse(
          PlusProfile({.facet = site, .plus_address = plus_address})));
  EXPECT_EQ(reserve.Get()->plus_address, plus_address);

  base::test::TestFuture<const PlusProfileOrError&> confirm;
  service.ConfirmPlusAddress(test_origin, plus_address, confirm.GetCallback());
  EXPECT_EQ(test_url_loader_factory.NumPending(), 1);
  test_url_loader_factory.SimulateResponseForPendingRequest(
      confirm_plus_address_endpoint,
      test::MakeCreationResponse(
          PlusProfile({.facet = site, .plus_address = plus_address})));
  EXPECT_EQ(confirm.Get()->plus_address, plus_address);
}

// Tests the PlusAddressService ability to make network requests.
class PlusAddressServicePolling : public PlusAddressServiceRequestsTest {
 public:
  PlusAddressServicePolling() {
    features()->Reset();
    features()->InitAndEnableFeatureWithParameters(
        plus_addresses::kFeature, {
                                      {"server-url", server_url.spec()},
                                      {"oauth-scope", "scope.example"},
                                      {"sync-with-server", "true"},
                                  });
    plus_addresses::RegisterProfilePrefs(pref_service_.registry());
  }

 protected:
  PrefService* prefs() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
};

// TODO: b/305696884 - Make this test simulate timer firing instead of
// directly calling SyncPlusAddressMapping.
TEST_F(PlusAddressServicePolling, CallsGetAllPlusAddresses) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressClient client(identity_test_env.identity_manager(),
                           test_shared_loader_factory);
  // The service starts the timer on construction and issues a request to
  // poll.
  PlusAddressService service(identity_test_env.identity_manager(), prefs(),
                             std::move(client));
  // Unblock the initial polling request.
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint, test::MakeListResponse({}));

  EXPECT_FALSE(service.IsPlusAddress("plus+foo@plus.plus"));
  EXPECT_FALSE(service.IsPlusAddress("plus+bar@plus.plus"));

  service.SyncPlusAddressMapping();
  // Note: The above call blocks until we provide a response to the request.
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint,
      test::MakeListResponse(
          {PlusProfile{.facet = "foo.com",
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

// Doesn't run on ChromeOS since ClearPrimaryAccount() doesn't exist for it.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PlusAddressServicePolling, PrimaryAccountCleared_TogglesPollingOff) {
  // Setup state where the PlusAddressService begins polling on creation.
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakePrimaryAccountAvailable("plus1@plus.plus",
                                                signin::ConsentLevel::kSignin);
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressClient client(identity_test_env.identity_manager(),
                           test_shared_loader_factory);
  PlusAddressService service(identity_test_env.identity_manager(), prefs(),
                             std::move(client));
  // Unblock initial poll.
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint, test::MakeListResponse({}));

  identity_test_env.ClearPrimaryAccount();
  service.SyncPlusAddressMapping();
  // The above doesn't block since it doesn't make network requests in this
  // state.

  // Toggle polling back on by signing into a primary account.
  identity_test_env.MakePrimaryAccountAvailable("plus2@plus.plus",
                                                signin::ConsentLevel::kSignin);
  service.SyncPlusAddressMapping();
  // TODO: b/305696884 - Remove above call to verify that observing
  // OnPrimaryAccountChanged will trigger this via CreateAndStartTimer().
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint,
      test::MakeListResponse({PlusProfile{
          .facet = "foo.com", .plus_address = "plus+foo@plus.plus"}}));
  url::Origin foo_origin = url::Origin::Create(GURL("https://foo.com"));
  ASSERT_TRUE(service.GetPlusAddress(foo_origin).has_value());
  EXPECT_EQ(service.GetPlusAddress(foo_origin).value(), "plus+foo@plus.plus");
  EXPECT_TRUE(service.IsPlusAddress("plus+foo@plus.plus"));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PlusAddressServicePolling, PrimaryRefreshTokenError_TogglesPollingOff) {
  // Setup state where the PlusAddressService begins polling on creation.
  signin::IdentityTestEnvironment identity_test_env;
  CoreAccountInfo primary_account =
      identity_test_env.MakePrimaryAccountAvailable(
          "plus1@plus.plus", signin::ConsentLevel::kSignin);
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressClient client(identity_test_env.identity_manager(),
                           test_shared_loader_factory);
  PlusAddressService service(identity_test_env.identity_manager(), prefs(),
                             std::move(client));
  // Unblock initial poll.
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint, test::MakeListResponse({}));

  // Toggle creation off by triggering an error for the primary refresh token.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  service.SyncPlusAddressMapping();
  // The above doesn't block since it doesn't make network requests in this
  // state.

  // Toggle creation back on by removing the error.
  identity_test_env.UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  service.SyncPlusAddressMapping();
  // TODO: b/305696884 - Remove above call to verify that observing
  // OnPrimaryAccountChanged will trigger this via CreateAndStartTimer().
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint,
      test::MakeListResponse({PlusProfile{
          .facet = "foo.com", .plus_address = "plus+foo@plus.plus"}}));
  url::Origin foo_origin = url::Origin::Create(GURL("https://foo.com"));
  ASSERT_TRUE(service.GetPlusAddress(foo_origin).has_value());
  EXPECT_EQ(service.GetPlusAddress(foo_origin).value(), "plus+foo@plus.plus");
  EXPECT_TRUE(service.IsPlusAddress("plus+foo@plus.plus"));
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
  // `SupportsPlusAddresses` should return `false`, even if there's a
  // signed-in user.
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

// Tests that PlusAddresses is "disabled" in the following states:
// - When a primary account is unset after login.
// - When a primary account's refresh token has an auth error.
//
// If PlusAddressService is "disabled" it should stop offering the feature,
// clear any local storage, and not issue network requests.
class PlusAddressServiceSignoutTest : public ::testing::Test {
 public:
  PlusAddressServiceSignoutTest() {
    secondary_account = identity_test_env_.MakeAccountAvailable(
        "beta@plus.plus", {signin::ConsentLevel::kSignin});
    primary_account = identity_test_env_.MakePrimaryAccountAvailable(
        "alpha@plus.plus", signin::ConsentLevel::kSignin);
  }

  CoreAccountInfo primary_account;
  AccountInfo secondary_account;

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_{plus_addresses::kFeature};
};

// Doesn't run on ChromeOS since ClearPrimaryAccount() doesn't exist for it.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PlusAddressServiceSignoutTest, PrimaryAccountCleared_TogglesIsEnabled) {
  PlusAddressService service(identity_test_env()->identity_manager());
  ASSERT_TRUE(service.is_enabled());

  // Verify behaviors expected when service is enabled.
  url::Origin site = url::Origin::Create(GURL("https://foo.com"));
  service.SavePlusAddress(site, "plus@plus.plus");
  EXPECT_TRUE(service.SupportsPlusAddresses(site));
  EXPECT_TRUE(service.GetPlusAddress(site));
  EXPECT_EQ(service.GetPlusAddress(site).value(), "plus@plus.plus");
  EXPECT_TRUE(service.IsPlusAddress("plus@plus.plus"));

  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(service.is_enabled());

  // Ensure that the local data is cleared on disabling.
  EXPECT_FALSE(service.SupportsPlusAddresses(site));
  EXPECT_FALSE(service.IsPlusAddress("plus@plus.plus"));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PlusAddressServiceSignoutTest,
       PrimaryRefreshTokenError_TogglesIsEnabled) {
  PlusAddressService service(identity_test_env()->identity_manager());
  ASSERT_TRUE(service.is_enabled());

  // Verify behaviors expected when service is enabled.
  url::Origin site = url::Origin::Create(GURL("https://foo.com"));
  service.SavePlusAddress(site, "plus@plus.plus");
  EXPECT_TRUE(service.SupportsPlusAddresses(site));
  EXPECT_TRUE(service.GetPlusAddress(site));
  EXPECT_EQ(service.GetPlusAddress(site).value(), "plus@plus.plus");
  EXPECT_TRUE(service.IsPlusAddress("plus@plus.plus"));

  // Setting to NONE doesn't disable the service.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  EXPECT_TRUE(service.is_enabled());

  // The PlusAddressService isn't disabled for secondary account auth errors.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary_account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(service.is_enabled());

  // Being in the "sync-paused" state results in this error.
  identity_test_env()->UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_FALSE(service.is_enabled());

  // Ensure that the local data is cleared on disabling.
  EXPECT_FALSE(service.SupportsPlusAddresses(site));
  EXPECT_FALSE(service.IsPlusAddress("plus@plus.plus"));
}

}  // namespace plus_addresses
