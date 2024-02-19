// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include <optional>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_http_client.h"
#include "components/plus_addresses/plus_address_prefs.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using SuggestionEvent = autofill::AutofillPlusAddressDelegate::SuggestionEvent;
using autofill::EqualsSuggestion;
using autofill::PopupItemId;
using autofill::Suggestion;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

auto IsSingleCreatePlusAddressSuggestion() {
  return ElementsAre(EqualsSuggestion(PopupItemId::kCreateNewPlusAddress));
}

auto IsSingleFillPlusAddressSuggestion(std::string_view address) {
  return ElementsAre(EqualsSuggestion(PopupItemId::kFillExistingPlusAddress,
                                      /*main_text=*/base::UTF8ToUTF16(address),
                                      Suggestion::Icon::kPlusAddress));
}

}  // namespace

namespace plus_addresses {

class MockPlusAddressService : public PlusAddressService {
 public:
  MOCK_METHOD(void, SyncPlusAddressMapping, ());
  void HandlePollingErrorForTesting(PlusAddressRequestError error) {
    return this->HandlePollingError(error);
  }
  int initial_poll_retry_attempt() { return initial_poll_retry_attempt_; }
  std::optional<bool> account_is_forbidden() { return account_is_forbidden_; }
};

TEST(PlusAddressService, HandlePollingError_NoopWhenFlagDisabled) {
  testing::StrictMock<MockPlusAddressService> service;
  // Make an error that we would attempt to retry.
  PlusAddressRequestError error(PlusAddressRequestErrorType::kNetworkError);
  error.set_http_response_code(403);
  EXPECT_CALL(service, SyncPlusAddressMapping()).Times(0);
  service.HandlePollingErrorForTesting(error);
  EXPECT_FALSE(service.account_is_forbidden().has_value());
}

TEST(PlusAddressService, HandlePollingError_NoopForNonNetworkError) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFeature, {{features::kDisableForForbiddenUsers.name, "true"}});
  testing::StrictMock<MockPlusAddressService> service;
  EXPECT_CALL(service, SyncPlusAddressMapping()).Times(0);
  service.HandlePollingErrorForTesting(
      PlusAddressRequestError(PlusAddressRequestErrorType::kOAuthError));
  EXPECT_FALSE(service.account_is_forbidden().has_value());
  service.HandlePollingErrorForTesting(
      PlusAddressRequestError(PlusAddressRequestErrorType::kParsingError));
  EXPECT_FALSE(service.account_is_forbidden().has_value());
}

TEST(PlusAddressService,
     HandlePollingError_NoopWhenNetworkErrorMissingResponseCode) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFeature, {{features::kDisableForForbiddenUsers.name, "true"}});
  testing::StrictMock<MockPlusAddressService> service;
  PlusAddressRequestError error(PlusAddressRequestErrorType::kNetworkError);
  EXPECT_CALL(service, SyncPlusAddressMapping()).Times(0);
  service.HandlePollingErrorForTesting(error);
  EXPECT_FALSE(service.account_is_forbidden().has_value());
}

TEST(PlusAddressService, HandlePollingError_NoopForNetworkErrorThatArent403) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFeature, {{features::kDisableForForbiddenUsers.name, "true"}});
  testing::StrictMock<MockPlusAddressService> service;
  PlusAddressRequestError error(PlusAddressRequestErrorType::kNetworkError);
  error.set_http_response_code(404);
  EXPECT_CALL(service, SyncPlusAddressMapping()).Times(0);
  service.HandlePollingErrorForTesting(error);
  EXPECT_FALSE(service.account_is_forbidden().has_value());
}

TEST(PlusAddressService, HandlePollingError_IncrementsRetryCounter) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFeature, {{features::kDisableForForbiddenUsers.name, "true"}});
  testing::StrictMock<MockPlusAddressService> service;
  // Make an error that we would attempt to retry.
  PlusAddressRequestError error(PlusAddressRequestErrorType::kNetworkError);
  error.set_http_response_code(403);
  EXPECT_EQ(service.initial_poll_retry_attempt(), 0);
  EXPECT_CALL(service, SyncPlusAddressMapping()).Times(1);
  service.HandlePollingErrorForTesting(error);
  EXPECT_FALSE(service.account_is_forbidden().has_value());
  EXPECT_EQ(service.initial_poll_retry_attempt(), 1);
}

TEST(PlusAddressService, HandlePollingError_SetsAccountIsForbidden) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeatureWithParameters(
      features::kFeature, {{features::kDisableForForbiddenUsers.name, "true"}});
  testing::StrictMock<MockPlusAddressService> service;
  // Make an error that we would attempt to retry.
  PlusAddressRequestError error(PlusAddressRequestErrorType::kNetworkError);
  error.set_http_response_code(403);
  // Initial error handling attempts to retry the request.
  EXPECT_CALL(service, SyncPlusAddressMapping()).Times(1);
  service.HandlePollingErrorForTesting(error);
  EXPECT_FALSE(service.account_is_forbidden().has_value());
  // If still failing with 403 after 1 retry, mark account as forbidden.
  EXPECT_CALL(service, SyncPlusAddressMapping()).Times(0);
  service.HandlePollingErrorForTesting(error);
  EXPECT_TRUE(service.account_is_forbidden().has_value());
  EXPECT_TRUE(service.account_is_forbidden().value());
}

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
  EXPECT_EQ(service.GetPlusAddress(url::Origin()), std::nullopt);
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
      url::Origin::Create(GURL("https://test.example")),
      /*is_off_the_record=*/false));
}

// Ensure `SupportsPlusAddresses` is false without a server URL.
TEST_F(PlusAddressServiceTest, SupportsPlusAddressNoServer) {
  // Enable the feature, but do not provide a server URL, which indicates no
  // suggestion should be shown.
  base::test::ScopedFeatureList scoped_feature_list{features::kFeature};
  PlusAddressService service;
  EXPECT_FALSE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://test.example")),
      /*is_off_the_record=*/false));
}

// Tests for the suggestion label overrides. These tests are not in the
// enabled/disabled fixtures as they vary parameters.
TEST_F(PlusAddressServiceTest, SuggestionLabelOverride) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Setting the override should result in echoing the override back.
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kFeature,
      {{features::kEnterprisePlusAddressSuggestionLabelOverride.name,
        "mattwashere"}});
  PlusAddressService service;
  EXPECT_EQ(service.GetCreateSuggestionLabel(), u"mattwashere");
}

TEST_F(PlusAddressServiceTest, LabelOverrideWithSpaces) {
  base::test::ScopedFeatureList scoped_feature_list;
  // Setting the override should result in echoing the override back.
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kFeature,
      {{features::kEnterprisePlusAddressSuggestionLabelOverride.name,
        "matt was here"}});
  PlusAddressService service;
  EXPECT_EQ(service.GetCreateSuggestionLabel(), u"matt was here");
}

TEST_F(PlusAddressServiceTest, NoAccountPlusAddressCreation) {
  signin::IdentityTestEnvironment identity_test_env;
  PlusAddressService service(identity_test_env.identity_manager());
  const url::Origin no_subdomain_origin =
      url::Origin::Create(GURL("https://test.example"));

  base::MockOnceCallback<void(const PlusProfileOrError&)> reserve_callback;
  base::MockOnceCallback<void(const PlusProfileOrError&)> confirm_callback;
  // Ensure that the lambdas aren't called since there is no signed-in account.
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

  base::MockOnceCallback<void(const PlusProfileOrError&)> reserve_callback;
  base::MockOnceCallback<void(const PlusProfileOrError&)> confirm_callback;
  // Ensure that the lambdas aren't called since there is no signed-in account.
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
        features::kFeature,
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

TEST_F(PlusAddressServiceRequestsTest, ReservePlusAddress_ReturnsUnconfirmed) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressService service(
      identity_test_env.identity_manager(), nullptr,
      PlusAddressHttpClient(identity_test_env.identity_manager(),
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
      PlusAddressHttpClient(identity_test_env.identity_manager(),
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
      PlusAddressHttpClient(identity_test_env.identity_manager(),
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
      PlusAddressHttpClient(identity_test_env.identity_manager(),
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
      PlusAddressHttpClient(identity_test_env.identity_manager(),
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
      PlusAddressHttpClient(identity_test_env.identity_manager(),
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
      PlusAddressHttpClient(identity_test_env.identity_manager(),
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
        features::kFeature, {
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

  PlusAddressHttpClient client(identity_test_env.identity_manager(),
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

TEST_F(PlusAddressServicePolling,
       DisableForForbiddenUsers_Enabled_404sDontDisableFeature) {
  features()->Reset();
  features()->InitAndEnableFeatureWithParameters(
      features::kFeature, {
                              {"server-url", server_url.spec()},
                              {"oauth-scope", "scope.example"},
                              {"sync-with-server", "true"},
                              {"disable-for-forbidden-users", "true"},
                          });
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressHttpClient client(identity_test_env.identity_manager(),
                               test_shared_loader_factory);
  // The service starts the timer on construction and issues a request to
  // poll.
  PlusAddressService service(identity_test_env.identity_manager(), prefs(),
                             std::move(client));
  EXPECT_TRUE(service.is_enabled());
  // Unblock the initial polling request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 1);
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint, "", net::HTTP_NOT_FOUND);
  EXPECT_TRUE(service.is_enabled());
}

TEST_F(PlusAddressServicePolling,
       DisableForForbiddenUsers_Enabled_403sDisableFeature) {
  features()->Reset();
  features()->InitAndEnableFeatureWithParameters(
      features::kFeature, {
                              {"server-url", server_url.spec()},
                              {"oauth-scope", "scope.example"},
                              {"sync-with-server", "true"},
                              {"disable-for-forbidden-users", "true"},
                          });
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressHttpClient client(identity_test_env.identity_manager(),
                               test_shared_loader_factory);
  // The service starts the timer on construction and issues a request to
  // poll.
  PlusAddressService service(identity_test_env.identity_manager(), prefs(),
                             std::move(client));
  EXPECT_TRUE(service.is_enabled());
  // Unblock the initial polling request.
  ASSERT_EQ(test_url_loader_factory.NumPending(), 1);
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint, "", net::HTTP_FORBIDDEN);
  // Simulate failed responses for the successive retry requests
  for (int i = 0; i < service.MAX_INITIAL_POLL_RETRY_ATTEMPTS; i++) {
    EXPECT_TRUE(service.is_enabled());
    ASSERT_EQ(test_url_loader_factory.NumPending(), 1);
    test_url_loader_factory.SimulateResponseForPendingRequest(
        plus_profiles_endpoint, "", net::HTTP_FORBIDDEN);
  }
  // Service is finally disabled once retries are exhausted.
  EXPECT_FALSE(service.is_enabled());
}

TEST_F(PlusAddressServicePolling,
       DisableForForbiddenUsers_Disabled_403DoesntRetryOrDisableFeature) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressHttpClient client(identity_test_env.identity_manager(),
                               test_shared_loader_factory);
  // The service starts the timer on construction and issues a request to
  // poll.
  PlusAddressService service(identity_test_env.identity_manager(), prefs(),
                             std::move(client));
  EXPECT_TRUE(service.is_enabled());
  // Unblock the initial polling request.
  test_url_loader_factory.SimulateResponseForPendingRequest(
      plus_profiles_endpoint, "", net::HTTP_FORBIDDEN);
  EXPECT_EQ(test_url_loader_factory.NumPending(), 0);
  EXPECT_TRUE(service.is_enabled());
}

// Doesn't run on ChromeOS since ClearPrimaryAccount() doesn't exist for it.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PlusAddressServicePolling, PrimaryAccountCleared_TogglesPollingOff) {
  // Setup state where the PlusAddressService begins polling on creation.
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakePrimaryAccountAvailable("plus1@plus.plus",
                                                signin::ConsentLevel::kSignin);
  identity_test_env.SetAutomaticIssueOfAccessTokens(true);

  PlusAddressHttpClient client(identity_test_env.identity_manager(),
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

  PlusAddressHttpClient client(identity_test_env.identity_manager(),
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
    scoped_feature_list_.InitAndDisableFeature(features::kFeature);
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
      url::Origin::Create(GURL("https://test.example")),
      /*is_off_the_record=*/false));
}

TEST_F(PlusAddressServiceDisabledTest, DisabledFeatureLabel) {
  // Disabled feature? Show the default generic text.
  PlusAddressService service;
  EXPECT_EQ(service.GetCreateSuggestionLabel(), u"Lorem Ipsum");
}

class PlusAddressServiceEnabledTest : public PlusAddressServiceTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kFeature,
        {{features::kEnterprisePlusAddressServerUrl.name, "mattwashere"}});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PlusAddressServiceEnabledTest, NullIdentityManager) {
  // Without an identity manager, the `SupportsPlusAddresses` should return
  // `false`.
  PlusAddressService service;
  EXPECT_FALSE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://test.example")),
      /*is_off_the_record=*/false));
}

TEST_F(PlusAddressServiceEnabledTest, NoSignedInUser) {
  // Without a signed in user, the `SupportsPlusAddresses` function should
  // return `false`.
  signin::IdentityTestEnvironment identity_test_env;
  PlusAddressService service(identity_test_env.identity_manager());
  EXPECT_FALSE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://test.example")),
      /*is_off_the_record=*/false));
}

TEST_F(PlusAddressServiceEnabledTest, FullySupported) {
  // With a signed in user, the `SupportsPlusAddresses` function should return
  // `true`.
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  PlusAddressService service(identity_test_env.identity_manager());
  EXPECT_TRUE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://test.example")),
      /*is_off_the_record=*/false));
}

// `SupportsPlusAddresses` returns false when `origin` is included on
// `kPlusAddressExcludedSites` and true otherwise.
TEST_F(PlusAddressServiceEnabledTest, ExcludedSitesAreNotSupported) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kFeature,
      {{features::kEnterprisePlusAddressServerUrl.name, "mattwashere"},
       {features::kPlusAddressExcludedSites.name,
        "exclude.co.th,forbidden.com"}});

  PlusAddressService service(identity_test_env.identity_manager());
  // Verify that url not on the excluded site continues to work.
  EXPECT_TRUE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://test.example")),
      /*is_off_the_record=*/false));

  // Sites on excluded list are not supported.
  EXPECT_FALSE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://www.forbidden.com")),
      /*is_off_the_record=*/false));
  EXPECT_FALSE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://www.exclude.co.th")),
      /*is_off_the_record=*/false));

  // Excluded site with different subdomain are also not supported.
  EXPECT_FALSE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://myaccount.forbidden.com")),
      /*is_off_the_record=*/false));
}

// `SupportsPlusAddresses` returns false when `origin` scheme is not http or
// https.
TEST_F(PlusAddressServiceEnabledTest, NonHTTPSchemesAreNotSupported) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  PlusAddressService service(identity_test_env.identity_manager());
  EXPECT_TRUE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("http://test.example")),
      /*is_off_the_record=*/false));
  EXPECT_FALSE(
      service.SupportsPlusAddresses(url::Origin::Create(GURL("other://hello")),
                                    /*is_off_the_record=*/false));
}

// `SupportsPlusAddresses` returns false when `origin` is opaque.
TEST_F(PlusAddressServiceEnabledTest, OpaqueOriginIsNotSupported) {
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  PlusAddressService service(identity_test_env.identity_manager());
  url::Origin origin;
  EXPECT_FALSE(service.SupportsPlusAddresses(origin, false));
}

TEST_F(PlusAddressServiceEnabledTest, OTRWithNoExistingAddress) {
  // With a signed in user, an off-the-record session, and no existing address,
  // the `SupportsPlusAddresses` function should return `false`.
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  PlusAddressService service(identity_test_env.identity_manager());
  EXPECT_FALSE(service.SupportsPlusAddresses(
      url::Origin::Create(GURL("https://test.example")),
      /*is_off_the_record=*/true));
}

TEST_F(PlusAddressServiceEnabledTest, OTRWithExistingAddress) {
  // With a signed in user, an off-the-record session, and an existing address,
  // the `SupportsPlusAddresses` function should return `true`.
  signin::IdentityTestEnvironment identity_test_env;
  identity_test_env.MakeAccountAvailable("plus@plus.plus",
                                         {signin::ConsentLevel::kSignin});
  url::Origin site = url::Origin::Create(GURL("https://foo.com"));

  PlusAddressService service(identity_test_env.identity_manager());
  service.SavePlusAddress(site, "plus@plus.plus");

  EXPECT_TRUE(service.SupportsPlusAddresses(site, /*is_off_the_record=*/true));
}

TEST_F(PlusAddressServiceEnabledTest, DefaultLabel) {
  // Override not set? Show the default generic text.
  PlusAddressService service;
  EXPECT_EQ(service.GetCreateSuggestionLabel(), u"Lorem Ipsum");
}

TEST_F(PlusAddressServiceEnabledTest, NoIdentityServiceGetEmail) {
  PlusAddressService service;
  EXPECT_EQ(service.GetPrimaryEmail(), std::nullopt);
}

TEST_F(PlusAddressServiceEnabledTest, SignedOutGetEmail) {
  signin::IdentityTestEnvironment identity_test_env;
  PlusAddressService service(identity_test_env.identity_manager());
  EXPECT_EQ(service.GetPrimaryEmail(), std::nullopt);
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

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kFeature,
        {{features::kEnterprisePlusAddressServerUrl.name, "mattwashere"},
         {features::kEnterprisePlusAddressOAuthScope.name, "scope.example"}});
  }

  CoreAccountInfo primary_account;
  AccountInfo secondary_account;

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Doesn't run on ChromeOS since ClearPrimaryAccount() doesn't exist for it.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PlusAddressServiceSignoutTest, PrimaryAccountCleared_TogglesIsEnabled) {
  PlusAddressService service(identity_test_env()->identity_manager());
  ASSERT_TRUE(service.is_enabled());

  // Verify behaviors expected when service is enabled.
  url::Origin site = url::Origin::Create(GURL("https://foo.com"));
  service.SavePlusAddress(site, "plus@plus.plus");
  EXPECT_TRUE(service.SupportsPlusAddresses(site, /*is_off_the_record=*/false));
  EXPECT_TRUE(service.GetPlusAddress(site));
  EXPECT_EQ(service.GetPlusAddress(site).value(), "plus@plus.plus");
  EXPECT_TRUE(service.IsPlusAddress("plus@plus.plus"));

  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(service.is_enabled());

  // Ensure that the local data is cleared on disabling.
  EXPECT_FALSE(
      service.SupportsPlusAddresses(site, /*is_off_the_record=*/false));
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
  EXPECT_TRUE(service.SupportsPlusAddresses(site, /*is_off_the_record=*/false));
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
  EXPECT_FALSE(
      service.SupportsPlusAddresses(site, /*is_off_the_record=*/false));
  EXPECT_FALSE(service.IsPlusAddress("plus@plus.plus"));
}

// A test fixture with a `PlusAddressService` that is enabled to allow testing
// suggestion generation.
class PlusAddressSuggestionsTest : public PlusAddressServiceTest {
 public:
  PlusAddressSuggestionsTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kFeature, {{"server-url", "https://server.example"},
                             {"oauth-scope", "scope.example"}});
    identity_test_env_.MakePrimaryAccountAvailable(
        "plus@plus.plus", signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
  }

 protected:
  PlusAddressService& service() { return service_; }

  static constexpr std::string_view kPlusAddressSuggestionMetric =
      "Autofill.PlusAddresses.Suggestion.Events";

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
  PlusAddressService service_{identity_test_env_.identity_manager()};
};

// Tests that fill plus address suggestions are offered iff the value in the
// focused field matches the prefix of an existing plus address.
TEST_F(PlusAddressSuggestionsTest, SuggestionsForExistingPlusAddress) {
  base::HistogramTester histogram_tester;
  const auto origin = url::Origin::Create(GURL("https://foo.coom"));
  const std::string plus_address = "plus+plus@plus.plus";
  service().SavePlusAddress(origin, plus_address);

  // We offer filling if the field is empty.
  EXPECT_THAT(service().GetSuggestions(origin, /*is_off_the_record=*/false,
                                       /*focused_field_value=*/u""),
              IsSingleFillPlusAddressSuggestion(plus_address));
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kExistingPlusAddressSuggested, 1);

  // If the user types a letter and it matches the plus address (after
  // normalization), the plus address continues to be offered.
  EXPECT_THAT(service().GetSuggestions(origin, /*is_off_the_record=*/false,
                                       /*focused_field_value=*/u"P"),
              IsSingleFillPlusAddressSuggestion(plus_address));
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kExistingPlusAddressSuggested, 2);

  // If the value does not match the prefix of the plus address, nothing is
  // shown.
  EXPECT_THAT(service().GetSuggestions(origin, /*is_off_the_record=*/false,
                                       /*focused_field_value=*/u"pp"),
              IsEmpty());
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kExistingPlusAddressSuggested, 2);
}

// Tests that a create plus address suggestion is offered if there is no
// existing plus address for the domain and the field value is empty.
TEST_F(PlusAddressSuggestionsTest, SuggestionsForCreateNewPlusAddress) {
  base::HistogramTester histogram_tester;
  const auto origin = url::Origin::Create(GURL("https://foo.coom"));

  // We offer creation if the field is empty.
  EXPECT_THAT(service().GetSuggestions(origin, /*is_off_the_record=*/false,
                                       /*focused_field_value=*/u""),
              IsSingleCreatePlusAddressSuggestion());
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kCreateNewPlusAddressSuggested, 1);

  // If the field value is not empty, nothing is shown.
  EXPECT_THAT(service().GetSuggestions(origin, /*is_off_the_record=*/false,
                                       /*focused_field_value=*/u"some text"),
              IsEmpty());
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kCreateNewPlusAddressSuggested, 1);
}

// Tests that no suggestions are returned when plus address are disabled.
TEST_F(PlusAddressSuggestionsTest, NoSuggestionsWhenDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kFeature);

  EXPECT_THAT(service().GetSuggestions(
                  url::Origin::Create(GURL("https://foo.coom")),
                  /*is_off_the_record=*/false, /*focused_field_value=*/u""),
              IsEmpty());
}

}  // namespace plus_addresses
