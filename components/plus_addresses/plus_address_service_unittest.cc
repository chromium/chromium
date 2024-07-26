// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_service.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_http_client_impl.h"
#include "components/plus_addresses/plus_address_test_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/settings/fake_plus_address_setting_service.h"
#include "components/plus_addresses/webdata/plus_address_sync_util.h"
#include "components/plus_addresses/webdata/plus_address_table.h"
#include "components/plus_addresses/webdata/plus_address_webdata_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_backend.h"
#include "components/webdata/common/web_database_service.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {

using PasswordFormType = autofill::AutofillClient::PasswordFormType;
using SuggestionEvent = autofill::AutofillPlusAddressDelegate::SuggestionEvent;
using affiliations::FacetURI;
using autofill::AutofillSuggestionTriggerSource;
using autofill::EqualsSuggestion;
using autofill::Suggestion;
using autofill::SuggestionType;
using ::base::test::RunOnceCallback;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::UnorderedElementsAre;

constexpr char kPlusAddress[] = "plus+remote@plus.plus";

auto IsSingleCreatePlusAddressSuggestion() {
  std::vector<std::vector<Suggestion::Text>> labels;
  if constexpr (!BUILDFLAG(IS_ANDROID)) {
    labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_CREATE_SUGGESTION_SECONDARY_TEXT))}};
  }
  return ElementsAre(
      AllOf(EqualsSuggestion(SuggestionType::kCreateNewPlusAddress,
                             /*main_text=*/l10n_util::GetStringUTF16(
                                 IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT)),
            Field(&Suggestion::icon, Suggestion::Icon::kPlusAddress),
            Field(&Suggestion::feature_for_iph,
                  &feature_engagement::kIPHPlusAddressCreateSuggestionFeature),
#if BUILDFLAG(IS_ANDROID)
            Field(&Suggestion::iph_description_text,
                  l10n_util::GetStringUTF16(
                      IDS_PLUS_ADDRESS_CREATE_SUGGESTION_IPH_ANDROID)),
#endif  // BUILDFLAG(IS_ANDROID)
            Field(&Suggestion::labels, labels)));
}

auto EqualsFillPlusAddressSuggestion(std::string_view address) {
  std::vector<std::vector<Suggestion::Text>> labels;
  if constexpr (!BUILDFLAG(IS_ANDROID)) {
      labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
          IDS_PLUS_ADDRESS_FILL_SUGGESTION_SECONDARY_TEXT))}};
  }
  return AllOf(EqualsSuggestion(SuggestionType::kFillExistingPlusAddress,
                                /*main_text=*/base::UTF8ToUTF16(address)),
               Field(&Suggestion::icon, Suggestion::Icon::kPlusAddress),
               Field(&Suggestion::labels, labels));
}

auto IsSingleFillPlusAddressSuggestion(std::string_view address) {
  return ElementsAre(EqualsFillPlusAddressSuggestion(address));
}

url::Origin OriginFromFacet(const plus_addresses::PlusProfile::facet_t& facet) {
  return url::Origin::Create(GURL("https://" + absl::get<std::string>(facet)));
}

}  // namespace

namespace plus_addresses {

class MockPlusAddressServiceObserver : public PlusAddressService::Observer {
 public:
  MockPlusAddressServiceObserver() = default;
  ~MockPlusAddressServiceObserver() override = default;
  MOCK_METHOD(void,
              OnPlusAddressesChanged,
              (const std::vector<PlusAddressDataChange>&),
              (override));
  MOCK_METHOD(void, OnPlusAddressServiceShutdown, (), (override));
};

class PlusAddressServiceTest : public ::testing::Test {
 public:
  PlusAddressServiceTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    InitService();
  }

  testing::AssertionResult ExpectServiceToReturnSuggestions(
      const url::Origin& origin,
      bool is_off_the_record,
      autofill::AutofillClient::PasswordFormType focused_form_type,
      std::u16string_view focused_field_value,
      autofill::AutofillSuggestionTriggerSource trigger_source,
      const auto& matcher) {
    base::MockCallback<PlusAddressService::GetSuggestionsCallback> callback;
    int calls = 0;
    ON_CALL(callback, Run)
        .WillByDefault([&](std::vector<autofill::Suggestion> suggestions) {
          EXPECT_THAT(suggestions, matcher);
          ++calls;
        });
    service().GetSuggestions(origin, is_off_the_record, focused_form_type,
                             focused_field_value, trigger_source,
                             callback.Get());
    return calls == 1
               ? testing::AssertionSuccess()
               : (testing::AssertionFailure() << "Error fetching suggestions.");
  }

 protected:
  // Constants that cannot be created at compile time:
  const url::Origin kNoSubdomainOrigin =
      url::Origin::Create(GURL("https://test.example"));

  signin::IdentityTestEnvironment& identity_env() { return identity_test_env_; }
  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }
  PlusAddressService& service() { return *service_; }
  const scoped_refptr<network::SharedURLLoaderFactory>&
  shared_loader_factory() {
    return test_shared_loader_factory_;
  }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  network::TestURLLoaderFactory& url_loader_factory() {
    return test_url_loader_factory_;
  }

  affiliations::MockAffiliationService* mock_affiliation_service() {
    return &mock_affiliation_service_;
  }

  FakePlusAddressSettingService& setting_service() { return setting_service_; }

  // Forces (re-)initialization of the `PlusAddressService`, which can be useful
  // when classes override feature parameters.
  void InitService() {
    service_.emplace(identity_manager(), &setting_service_,
                     std::make_unique<PlusAddressHttpClientImpl>(
                         identity_manager(), shared_loader_factory()),
                     /*webdata_service=*/nullptr,
                     /*affiliation_service=*/
                     &mock_affiliation_service_,
                     /*feature_enabled_for_profile_check=*/
                     base::BindRepeating(&base::FeatureList::IsEnabled));
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  signin::IdentityTestEnvironment identity_test_env_;
  FakePlusAddressSettingService setting_service_;
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  data_decoder::test::InProcessDataDecoder decoder_;
  std::optional<PlusAddressService> service_;
};

TEST_F(PlusAddressServiceTest, BasicTest) {
  const PlusProfile profile = test::CreatePlusProfile();
  EXPECT_FALSE(service().IsPlusAddress(profile.plus_address));
  service().SavePlusProfile(profile);
  EXPECT_TRUE(service().IsPlusAddress(profile.plus_address));
  EXPECT_EQ(service().GetPlusAddress(profile.facet), profile.plus_address);
  EXPECT_EQ(service().GetPlusAddress(affiliations::FacetURI()), std::nullopt);
  EXPECT_EQ(service().GetPlusProfile(profile.facet)->plus_address,
            profile.plus_address);
}

TEST_F(PlusAddressServiceTest, GetPlusProfileByFacet) {
  const PlusProfile profile = test::CreatePlusProfile(/*use_full_domain=*/true);
  EXPECT_FALSE(service().IsPlusAddress(profile.plus_address));
  service().SavePlusProfile(profile);
  EXPECT_TRUE(service().IsPlusAddress(profile.plus_address));
  EXPECT_EQ(
      service().GetPlusProfile(
          affiliations::FacetURI::FromPotentiallyInvalidSpec("invalid facet")),
      std::nullopt);
  EXPECT_EQ(service().GetPlusProfile(
                absl::get<affiliations::FacetURI>(profile.facet)),
            profile);
}

TEST_F(PlusAddressServiceTest, DefaultShouldShowManualFallbackState) {
  // By default, the `ShouldShowManualFallback` function should return `false`.
  EXPECT_FALSE(service().ShouldShowManualFallback(kNoSubdomainOrigin,
                                                  /*is_off_the_record=*/false));
}

// Ensure `ShouldShowManualFallback` is false without a server URL.
TEST_F(PlusAddressServiceTest, ShouldShowManualFallbackNoServer) {
  // Enable the feature, but do not provide a server URL, which indicates no
  // suggestion should be shown.
  base::test::ScopedFeatureList scoped_feature_list{
      features::kPlusAddressesEnabled};
  InitService();
  EXPECT_FALSE(service().ShouldShowManualFallback(kNoSubdomainOrigin,
                                                  /*is_off_the_record=*/false));
}

TEST_F(PlusAddressServiceTest, NoAccountPlusAddressCreation) {
  base::MockOnceCallback<void(const PlusProfileOrError&)> reserve_callback;
  base::MockOnceCallback<void(const PlusProfileOrError&)> confirm_callback;
  // Ensure that the lambdas aren't called since there is no signed-in account.
  EXPECT_CALL(reserve_callback, Run).Times(0);
  EXPECT_CALL(confirm_callback, Run).Times(0);
  service().ReservePlusAddress(kNoSubdomainOrigin, reserve_callback.Get());
  service().ConfirmPlusAddress(kNoSubdomainOrigin, kPlusAddress,
                               confirm_callback.Get());
}

TEST_F(PlusAddressServiceTest, AbortPlusAddressCreation) {
  const std::string invalid_email = "plus";
  identity_env().MakeAccountAvailable(invalid_email,
                                      {signin::ConsentLevel::kSignin});
  InitService();

  base::MockOnceCallback<void(const PlusProfileOrError&)> reserve_callback;
  base::MockOnceCallback<void(const PlusProfileOrError&)> confirm_callback;
  // Ensure that the lambdas aren't called since there is no signed-in account.
  EXPECT_CALL(reserve_callback, Run).Times(0);
  EXPECT_CALL(confirm_callback, Run).Times(0);
  service().ReservePlusAddress(kNoSubdomainOrigin, reserve_callback.Get());
  service().ConfirmPlusAddress(kNoSubdomainOrigin, kPlusAddress,
                               confirm_callback.Get());
}

// Tests that GetPlusProfiles returns all cached plus profiles.
TEST_F(PlusAddressServiceTest, GetPlusProfiles) {
  PlusProfile profile1 = test::CreatePlusProfile();
  PlusProfile profile2 = test::CreatePlusProfile2();
  service().SavePlusProfile(profile1);
  service().SavePlusProfile(profile2);

  EXPECT_THAT(service().GetPlusProfiles(),
              UnorderedElementsAre(profile1, profile2));
}

// Tests the PlusAddressService ability to make network requests.
class PlusAddressServiceRequestsTest : public PlusAddressServiceTest {
 public:
  explicit PlusAddressServiceRequestsTest()
      : kPlusProfilesEndpoint(
            kServerUrl.Resolve(kServerPlusProfileEndpoint).spec()),
        kReservePlusAddressEndpoint(
            kServerUrl.Resolve(kServerReservePlusAddressEndpoint).spec()),
        kCreatePlusAddressEndpoint(
            kServerUrl.Resolve(kServerCreatePlusAddressEndpoint).spec()) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPlusAddressesEnabled, GetFieldTrialParams());
    identity_env().MakeAccountAvailable(kSigninAccount,
                                        {signin::ConsentLevel::kSignin});
    identity_env().SetAutomaticIssueOfAccessTokens(true);
    InitService();
  }

 protected:
  static constexpr std::string_view kSigninAccount = "plus@plus.plus";

  // Constants that cannot be created at compile time:
  const GURL kServerUrl = GURL("https://server.example");
  const std::string kPlusProfilesEndpoint;
  const std::string kReservePlusAddressEndpoint;
  const std::string kCreatePlusAddressEndpoint;

  base::FieldTrialParams GetFieldTrialParams() const {
    return {{"server-url", kServerUrl.spec()},
            {"oauth-scope", "scope.example"}};
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PlusAddressServiceRequestsTest, ReservePlusAddress_ReturnsUnconfirmed) {
  PlusProfile profile = test::CreatePlusProfile();
  base::test::TestFuture<const PlusProfileOrError&> future;
  service().ReservePlusAddress(OriginFromFacet(profile.facet),
                               future.GetCallback());

  // Check that the future callback is still blocked, and unblock it.
  profile.is_confirmed = false;
  ASSERT_FALSE(future.IsReady());
  url_loader_factory().SimulateResponseForPendingRequest(
      kReservePlusAddressEndpoint, test::MakeCreationResponse(profile));
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get()->plus_address, profile.plus_address);

  // The service should not save plus_address if it hasn't been confirmed yet.
  EXPECT_FALSE(service().IsPlusAddress(profile.plus_address));
}

TEST_F(PlusAddressServiceRequestsTest, ReservePlusAddress_ReturnsConfirmed) {
  PlusProfile profile = test::CreatePlusProfile();
  base::test::TestFuture<const PlusProfileOrError&> future;
  service().ReservePlusAddress(OriginFromFacet(profile.facet),
                               future.GetCallback());

  // Check that the future callback is still blocked, and unblock it.
  ASSERT_FALSE(future.IsReady());
  url_loader_factory().SimulateResponseForPendingRequest(
      kReservePlusAddressEndpoint, test::MakeCreationResponse(profile));
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get()->plus_address, profile.plus_address);

  // The service should save kPlusAddress if it has already been confirmed.
  EXPECT_TRUE(service().IsPlusAddress(profile.plus_address));
}

TEST_F(PlusAddressServiceRequestsTest, ReservePlusAddress_Fails) {
  base::test::TestFuture<const PlusProfileOrError&> future;
  service().ReservePlusAddress(kNoSubdomainOrigin, future.GetCallback());

  // Check that the future callback is still blocked, and unblock it.
  ASSERT_FALSE(future.IsReady());
  url_loader_factory().SimulateResponseForPendingRequest(
      kReservePlusAddressEndpoint, "", net::HTTP_BAD_REQUEST);
  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().has_value());
}

TEST_F(PlusAddressServiceRequestsTest, ConfirmPlusAddress_Successful) {
  const PlusProfile& profile = test::CreatePlusProfile();
  MockPlusAddressServiceObserver observer;
  service().AddObserver(&observer);
  EXPECT_CALL(observer,
              OnPlusAddressesChanged(ElementsAre(PlusAddressDataChange(
                  PlusAddressDataChange::Type::kAdd, profile))));
  base::test::TestFuture<const PlusProfileOrError&> future;
  service().ConfirmPlusAddress(OriginFromFacet(profile.facet),
                               profile.plus_address, future.GetCallback());

  // Check that the future callback is still blocked, and unblock it.
  ASSERT_FALSE(future.IsReady());
  url_loader_factory().SimulateResponseForPendingRequest(
      kCreatePlusAddressEndpoint, test::MakeCreationResponse(profile));
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get()->plus_address, profile.plus_address);
  // Verify that the kPlusAddress is saved when confirmation is successful.
  EXPECT_TRUE(service().IsPlusAddress(profile.plus_address));

  // Assert that ensuing calls to the same facet do not make a network request.
  base::test::TestFuture<const PlusProfileOrError&> second_future;
  service().ConfirmPlusAddress(OriginFromFacet(profile.facet),
                               profile.plus_address,
                               second_future.GetCallback());
  ASSERT_TRUE(second_future.IsReady());
  EXPECT_EQ(second_future.Get()->plus_address, profile.plus_address);
  service().RemoveObserver(&observer);
}

TEST_F(PlusAddressServiceRequestsTest, ConfirmPlusAddress_Fails) {
  ASSERT_FALSE(service().IsPlusAddress(kPlusAddress));

  base::test::TestFuture<const PlusProfileOrError&> future;
  service().ConfirmPlusAddress(kNoSubdomainOrigin, kPlusAddress,
                               future.GetCallback());

  // Check that the future callback is still blocked, and unblock it.
  ASSERT_FALSE(future.IsReady());
  url_loader_factory().SimulateResponseForPendingRequest(
      kCreatePlusAddressEndpoint, "", net::HTTP_BAD_REQUEST);
  ASSERT_TRUE(future.IsReady());

  // An error is propagated from the callback and kPlusAddress is not saved.
  EXPECT_FALSE(future.Get().has_value());
  EXPECT_FALSE(service().IsPlusAddress(kPlusAddress));
}

// Doesn't run on ChromeOS since ClearPrimaryAccount() doesn't exist for it.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PlusAddressServiceRequestsTest,
       PrimaryAccountCleared_TogglesPlusAddressCreationOff) {
  // Toggle creation off by removing the primary account.
  identity_env().ClearPrimaryAccount();

  // Verify that Plus Address creation doesn't occur.
  PlusProfile profile = test::CreatePlusProfile();
  profile.is_confirmed = false;
  service().ReservePlusAddress(OriginFromFacet(profile.facet),
                               base::DoNothing());
  service().ConfirmPlusAddress(OriginFromFacet(profile.facet),
                               profile.plus_address, base::DoNothing());
  EXPECT_EQ(url_loader_factory().NumPending(), 0);

  // Toggle creation back on by signing in again.
  identity_env().MakePrimaryAccountAvailable("plus@plus.plus",
                                             signin::ConsentLevel::kSignin);

  // Verify that Plus Address creation occurs and makes a network request.
  base::test::TestFuture<const PlusProfileOrError&> reserve;
  service().ReservePlusAddress(OriginFromFacet(profile.facet),
                               reserve.GetCallback());
  EXPECT_EQ(url_loader_factory().NumPending(), 1);
  url_loader_factory().SimulateResponseForPendingRequest(
      kReservePlusAddressEndpoint, test::MakeCreationResponse(profile));
  EXPECT_EQ(reserve.Get()->plus_address, profile.plus_address);

  base::test::TestFuture<const PlusProfileOrError&> confirm;
  service().ConfirmPlusAddress(OriginFromFacet(profile.facet),
                               profile.plus_address, confirm.GetCallback());
  EXPECT_EQ(url_loader_factory().NumPending(), 1);
  profile.is_confirmed = true;
  url_loader_factory().SimulateResponseForPendingRequest(
      kCreatePlusAddressEndpoint, test::MakeCreationResponse(profile));
  EXPECT_EQ(confirm.Get()->plus_address, profile.plus_address);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that if an account error happens while a server request is ongoing,
// the request ends in an error and the eventual server response is ignored.
TEST_F(PlusAddressServiceRequestsTest,
       PrimaryRefreshTokenError_ResetsHttpRequests) {
  base::test::ScopedFeatureList sync_feature_{
      GetSyncPlusAddressFeatureForTests()};
  PlusProfile profile = test::CreatePlusProfile();
  base::test::TestFuture<const PlusProfileOrError&> future;
  service().ReservePlusAddress(OriginFromFacet(profile.facet),
                               future.GetCallback());

  // Check that the future callback is still blocked, and unblock it.
  ASSERT_FALSE(future.IsReady());

  // Simulate an auth error happening while the server response is still
  // pending.
  const CoreAccountInfo primary_account =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  identity_env().UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // The auth change calls the callback with an error.
  ASSERT_TRUE(future.IsReady());
  EXPECT_THAT(future.Get(), base::test::ErrorIs(PlusAddressRequestError(
                                PlusAddressRequestErrorType::kUserSignedOut)));

  // Nothing happens once the server responds.
  url_loader_factory().SimulateResponseForPendingRequest(
      kReservePlusAddressEndpoint, test::MakeCreationResponse(profile));
  EXPECT_THAT(service().GetPlusProfiles(), IsEmpty());
}

TEST_F(PlusAddressServiceRequestsTest,
       PrimaryRefreshTokenError_TogglesPlusAddressCreationOff) {
  CoreAccountInfo primary_account =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  // Toggle creation off by triggering an error for the primary refresh token.
  identity_env().UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  // Verify that Plus Address creation doesn't occur.
  PlusProfile profile = test::CreatePlusProfile();
  service().ReservePlusAddress(OriginFromFacet(profile.facet),
                               base::DoNothing());
  service().ConfirmPlusAddress(OriginFromFacet(profile.facet),
                               profile.plus_address, base::DoNothing());
  EXPECT_EQ(url_loader_factory().NumPending(), 0);

  // Toggle creation back on by removing the error.
  identity_env().UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));

  // Verify that Plus Address creation occurs and makes a network request.
  base::test::TestFuture<const PlusProfileOrError&> reserve;
  service().ReservePlusAddress(OriginFromFacet(profile.facet),
                               reserve.GetCallback());
  EXPECT_EQ(url_loader_factory().NumPending(), 1);
  profile.is_confirmed = false;
  url_loader_factory().SimulateResponseForPendingRequest(
      kReservePlusAddressEndpoint, test::MakeCreationResponse(profile));
  EXPECT_EQ(reserve.Get()->plus_address, profile.plus_address);

  base::test::TestFuture<const PlusProfileOrError&> confirm;
  service().ConfirmPlusAddress(OriginFromFacet(profile.facet),
                               profile.plus_address, confirm.GetCallback());
  EXPECT_EQ(url_loader_factory().NumPending(), 1);
  profile.is_confirmed = true;
  url_loader_factory().SimulateResponseForPendingRequest(
      kCreatePlusAddressEndpoint, test::MakeCreationResponse(profile));
  EXPECT_EQ(confirm.Get()->plus_address, profile.plus_address);
}

// Tests that ongoing network requests are cancelled on signout.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PlusAddressServiceRequestsTest, OngoingRequestsCancelledOnSignout) {
  base::test::TestFuture<const PlusProfileOrError&> future;
  service().ReservePlusAddress(kNoSubdomainOrigin, future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  EXPECT_EQ(url_loader_factory().NumPending(), 1);
  identity_env().ClearPrimaryAccount();
  EXPECT_EQ(url_loader_factory().NumPending(), 0);
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), base::unexpected(PlusAddressRequestError(
                              PlusAddressRequestErrorType::kUserSignedOut)));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class PlusAddressHttpForbiddenResponseTest
    : public PlusAddressServiceRequestsTest {
 public:
  PlusAddressHttpForbiddenResponseTest() {
    base::FieldTrialParams params =
        PlusAddressServiceRequestsTest::GetFieldTrialParams();
    params[features::kDisableForForbiddenUsers.name] = "true";
    features_.InitAndEnableFeatureWithParameters(
        features::kPlusAddressesEnabled, params);
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Tests that two `HTTP_FORBIDDEN` responses and no successful network request
// lead to a disabled service.
TEST_F(PlusAddressHttpForbiddenResponseTest, RepeatedHttpForbiddenFromConfirm) {
  const std::string kPlusAddress = "plus+remote@plus.plus";
  ASSERT_FALSE(service().IsPlusAddress(kPlusAddress));

  // The service remains enabled after a single `HTTP_FORBIDDEN` response.
  service().ConfirmPlusAddress(kNoSubdomainOrigin, kPlusAddress,
                               base::DoNothing());
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kCreatePlusAddressEndpoint, "", net::HTTP_FORBIDDEN));
  EXPECT_TRUE(service().IsEnabled());

  // A second `HTTP_FORBIDDEN` responses disables it.
  service().ConfirmPlusAddress(kNoSubdomainOrigin, kPlusAddress,
                               base::DoNothing());
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kCreatePlusAddressEndpoint, "", net::HTTP_FORBIDDEN));
  EXPECT_FALSE(service().IsEnabled());
}

// Tests that two `HTTP_FORBIDDEN` responses and no successful network request
// do not lead to a disabled service unless the feature param is set.
TEST_F(PlusAddressHttpForbiddenResponseTest,
       RepeatedHttpForbiddenFromConfirmWithDisabledParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kPlusAddressesEnabled,
      PlusAddressServiceRequestsTest::GetFieldTrialParams());

  const std::string kPlusAddress = "plus+remote@plus.plus";
  ASSERT_FALSE(service().IsPlusAddress(kPlusAddress));

  // The service remains enabled after a single `HTTP_FORBIDDEN` response.
  service().ConfirmPlusAddress(kNoSubdomainOrigin, kPlusAddress,
                               base::DoNothing());
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kCreatePlusAddressEndpoint, "", net::HTTP_FORBIDDEN));
  EXPECT_TRUE(service().IsEnabled());

  // A second `HTTP_FORBIDDEN` responses disables it.
  service().ConfirmPlusAddress(kNoSubdomainOrigin, kPlusAddress,
                               base::DoNothing());
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kCreatePlusAddressEndpoint, "", net::HTTP_FORBIDDEN));
  EXPECT_TRUE(service().IsEnabled());
}

// Tests that two `HTTP_FORBIDDEN` responses and no successful network request
// lead to a disabled service and that other network errors do not have an
// impact.
TEST_F(PlusAddressHttpForbiddenResponseTest, OtherErrorsHaveNoEffect) {
  const std::string kPlusAddress = "plus+remote@plus.plus";
  ASSERT_FALSE(service().IsPlusAddress(kPlusAddress));

  // The service remains enabled after a single `HTTP_FORBIDDEN` response.
  service().ReservePlusAddress(kNoSubdomainOrigin, base::DoNothing());
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kReservePlusAddressEndpoint, "", net::HTTP_FORBIDDEN));
  EXPECT_TRUE(service().IsEnabled());

  // A failure that is not `HTTP_FORBIDDEN` does not disable the service.
  service().ReservePlusAddress(kNoSubdomainOrigin, base::DoNothing());
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kReservePlusAddressEndpoint, "", net::HTTP_REQUEST_TIMEOUT));
  EXPECT_TRUE(service().IsEnabled());

  // But a second `HTTP_FORBIDDEN` does.
  service().ReservePlusAddress(kNoSubdomainOrigin, base::DoNothing());
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kReservePlusAddressEndpoint, "", net::HTTP_FORBIDDEN));
  EXPECT_FALSE(service().IsEnabled());
}

// Tests a single successful response prevents later `HTTP_FORBIDDEN` responses
// from disabling the service.
TEST_F(PlusAddressHttpForbiddenResponseTest, NoDisablingAfterSuccess) {
  const PlusProfile profile1 = test::CreatePlusProfile();
  ASSERT_FALSE(service().IsPlusAddress(profile1.plus_address));

  // The service remains enabled after a single `HTTP_FORBIDDEN` response.
  service().ConfirmPlusAddress(OriginFromFacet(profile1.facet),
                               profile1.plus_address, base::DoNothing());
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kCreatePlusAddressEndpoint, "", net::HTTP_FORBIDDEN));
  EXPECT_TRUE(service().IsEnabled());

  // After a single successful call ...
  service().ConfirmPlusAddress(OriginFromFacet(profile1.facet),
                               profile1.plus_address, base::DoNothing());
  ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
      kCreatePlusAddressEndpoint, test::MakeCreationResponse(profile1)));
  EXPECT_TRUE(service().IsPlusAddress(profile1.plus_address));

  // ... even repeated `HTTP_FORBIDDEN` responses do not disable the service.
  const PlusProfile profile2 = test::CreatePlusProfile2();
  for (int i = 0; i < 5; ++i) {
    SCOPED_TRACE(::testing::Message() << "Iteration #" << 1);
    // But a second `HTTP_FORBIDDEN` does.
    service().ConfirmPlusAddress(OriginFromFacet(profile2.facet),
                                 profile2.plus_address, base::DoNothing());
    ASSERT_TRUE(url_loader_factory().SimulateResponseForPendingRequest(
        kCreatePlusAddressEndpoint, "", net::HTTP_FORBIDDEN));
    EXPECT_TRUE(service().IsEnabled());
  }
}

// Tests the PlusAddressService ability to make network requests.
class PlusAddressServicePolling : public PlusAddressServiceRequestsTest {
 public:
  PlusAddressServicePolling() {
    base::FieldTrialParams params =
        PlusAddressServiceRequestsTest::GetFieldTrialParams();
    params["sync-with-server"] = "true";
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kPlusAddressesEnabled, params);
    InitService();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(PlusAddressServicePolling, CallsGetAllPlusAddresses) {
  // The service starts the timer on construction and issues a request to
  // poll. Unblock the initial polling request.
  url_loader_factory().SimulateResponseForPendingRequest(
      kPlusProfilesEndpoint, test::MakeListResponse({}));

  const PlusProfile profile1 = test::CreatePlusProfile();
  const PlusProfile profile2 = test::CreatePlusProfile2();
  EXPECT_FALSE(service().IsPlusAddress(profile1.plus_address));
  EXPECT_FALSE(service().IsPlusAddress(profile2.plus_address));

  task_environment().FastForwardBy(
      features::kEnterprisePlusAddressTimerDelay.Get() + base::Seconds(1));
  EXPECT_EQ(url_loader_factory().NumPending(), 1);
  url_loader_factory().SimulateResponseForPendingRequest(
      kPlusProfilesEndpoint, test::MakeListResponse({profile1, profile2}));

  // The service's mapping should be updated now.
  for (const PlusProfile& profile : {profile1, profile2}) {
    SCOPED_TRACE(testing::Message() << profile.plus_address);
    EXPECT_EQ(service().GetPlusAddress(profile.facet), profile.plus_address);
    EXPECT_TRUE(service().IsPlusAddress(profile.plus_address));
  }
}

TEST_F(PlusAddressServicePolling,
       DisableForForbiddenUsers_Enabled_404sDontDisableFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kPlusAddressesEnabled,
      {
          {"server-url", kServerUrl.spec()},
          {"oauth-scope", "scope.example"},
          {"sync-with-server", "true"},
          {"disable-for-forbidden-users", "true"},
      });
  InitService();

  EXPECT_TRUE(service().IsEnabled());
  // Unblock the initial polling request.
  ASSERT_EQ(url_loader_factory().NumPending(), 1);
  url_loader_factory().SimulateResponseForPendingRequest(
      kPlusProfilesEndpoint, "", net::HTTP_NOT_FOUND);
  EXPECT_TRUE(service().IsEnabled());
}

TEST_F(PlusAddressServicePolling,
       DisableForForbiddenUsers_Enabled_403sDisableFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kPlusAddressesEnabled,
      {
          {"server-url", kServerUrl.spec()},
          {"oauth-scope", "scope.example"},
          {"sync-with-server", "true"},
          {"disable-for-forbidden-users", "true"},
      });
  InitService();

  EXPECT_TRUE(service().IsEnabled());
  // Unblock the initial polling request.
  ASSERT_EQ(url_loader_factory().NumPending(), 1);
  url_loader_factory().SimulateResponseForPendingRequest(
      kPlusProfilesEndpoint, "", net::HTTP_FORBIDDEN);
  // Simulate failed responses for the successive retry requests
  for (int i = 0; i < PlusAddressService::kMaxHttpForbiddenResponses; i++) {
    EXPECT_TRUE(service().IsEnabled());
    ASSERT_EQ(url_loader_factory().NumPending(), 1);
    url_loader_factory().SimulateResponseForPendingRequest(
        kPlusProfilesEndpoint, "", net::HTTP_FORBIDDEN);
  }
  // Service is finally disabled once retries are exhausted.
  EXPECT_FALSE(service().IsEnabled());
}

TEST_F(PlusAddressServicePolling,
       DisableForForbiddenUsers_Disabled_403DoesntRetryOrDisableFeature) {
  EXPECT_TRUE(service().IsEnabled());
  // Unblock the initial polling request.
  url_loader_factory().SimulateResponseForPendingRequest(
      kPlusProfilesEndpoint, "", net::HTTP_FORBIDDEN);
  EXPECT_EQ(url_loader_factory().NumPending(), 0);
  EXPECT_TRUE(service().IsEnabled());
}

// Doesn't run on ChromeOS since ClearPrimaryAccount() doesn't exist for it.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PlusAddressServicePolling, PrimaryAccountCleared_TogglesPollingOff) {
  // Unblock initial poll.
  url_loader_factory().SimulateResponseForPendingRequest(
      kPlusProfilesEndpoint, test::MakeListResponse({}));
  identity_env().ClearPrimaryAccount();

  // Toggle polling back on by signing into a primary account.
  identity_env().MakePrimaryAccountAvailable("plus2@plus.plus",
                                             signin::ConsentLevel::kSignin);
  const PlusProfile profile = test::CreatePlusProfile();
  url_loader_factory().SimulateResponseForPendingRequest(
      kPlusProfilesEndpoint, test::MakeListResponse({profile}));
  EXPECT_EQ(service().GetPlusAddress(profile.facet), profile.plus_address);
  EXPECT_TRUE(service().IsPlusAddress(profile.plus_address));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PlusAddressServicePolling, PrimaryRefreshTokenError_TogglesPollingOff) {
  CoreAccountInfo primary_account =
      identity_manager()->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  // Unblock initial poll.
  url_loader_factory().SimulateResponseForPendingRequest(
      kPlusProfilesEndpoint, test::MakeListResponse({}));

  // Toggle creation off by triggering an error for the primary refresh token.
  identity_env().UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  task_environment().RunUntilIdle();
  // No network requests are expected because the service is not enabled.
  EXPECT_EQ(url_loader_factory().NumPending(), 0);

  // Toggle creation back on by removing the error.
  identity_env().UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  task_environment().RunUntilIdle();

  EXPECT_EQ(url_loader_factory().NumPending(), 1);
  const PlusProfile profile = test::CreatePlusProfile();
  url_loader_factory().SimulateResponseForPendingRequest(
      kPlusProfilesEndpoint, test::MakeListResponse({profile}));
  EXPECT_EQ(service().GetPlusAddress(profile.facet), profile.plus_address);
  EXPECT_TRUE(service().IsPlusAddress("plus+foo@plus.plus"));
}

// Tests that communication with `PlusAddressTable` works.
class PlusAddressServiceWebDataTest : public ::testing::Test {
 protected:
  PlusAddressServiceWebDataTest() {
    // Create an in-memory PlusAddressTable fully operating on the UI sequence.
    webdatabase_service_ = base::MakeRefCounted<WebDatabaseService>(
        base::FilePath(WebDatabase::kInMemoryPath),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    webdatabase_service_->AddTable(std::make_unique<PlusAddressTable>());
    webdatabase_service_->LoadDatabase();
    plus_webdata_service_ = base::MakeRefCounted<PlusAddressWebDataService>(
        webdatabase_service_, base::SingleThreadTaskRunner::GetCurrentDefault(),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    plus_webdata_service_->Init(base::DoNothing());
    // Even though `PlusAddressTable` operates on the UI sequence in this test,
    // it is still implemented using `PostTask()`.
    task_environment_.RunUntilIdle();
    // Initialize the `service_` using the `plus_webdata_service_`.
    service_.emplace(
        identity_test_env_.identity_manager(), &setting_service_,
        std::make_unique<PlusAddressHttpClientImpl>(
            /*identity_manager=*/identity_test_env_.identity_manager(),
            /*url_loader_factory=*/nullptr),
        plus_webdata_service_,
        /*affiliation_service=*/&mock_affiliation_service_,
        /*feature_enabled_for_profile_check=*/
        base::BindRepeating(&base::FeatureList::IsEnabled));
  }

  PlusAddressService& service() { return *service_; }

  PlusAddressTable& table() {
    return *PlusAddressTable::FromWebDatabase(
        webdatabase_service_->GetBackend()->database());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  FakePlusAddressSettingService setting_service_;
  scoped_refptr<WebDatabaseService> webdatabase_service_;
  scoped_refptr<PlusAddressWebDataService> plus_webdata_service_;
  NiceMock<affiliations::MockAffiliationService> mock_affiliation_service_;
  // Except briefly during initialisation, it always has a value.
  std::optional<PlusAddressService> service_;
};

TEST_F(PlusAddressServiceWebDataTest, OnWebDataChangedBySync) {
  const PlusProfile profile1 =
      test::CreatePlusProfile(/*use_full_domain=*/true);
  const PlusProfile profile2 =
      test::CreatePlusProfile2(/*use_full_domain=*/true);
  // Simulate adding and removing profiles to the database directly, as sync
  // would. This triggers `OnWebDataChangedBySync()`. Prior to the notification,
  // `service()` has no way of knowing about this data.
  table().AddOrUpdatePlusProfile(profile1);
  table().AddOrUpdatePlusProfile(profile2);

  service().SavePlusProfile(profile1);
  EXPECT_THAT(service().GetPlusProfiles(), ElementsAre(profile1));

  MockPlusAddressServiceObserver observer;
  service().AddObserver(&observer);
  // Simulate incoming changes from sync. Note that `profile1` already exists in
  // the service and therefore should not be included as part of the updates
  // sent to the `observer`.
  EXPECT_CALL(observer,
              OnPlusAddressesChanged(ElementsAre(PlusAddressDataChange(
                  PlusAddressDataChange::Type::kAdd, profile2))));
  service().OnWebDataChangedBySync(
      {PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile1),
       PlusAddressDataChange(PlusAddressDataChange::Type::kAdd, profile2)});
  EXPECT_THAT(service().GetPlusProfiles(),
              UnorderedElementsAre(profile1, profile2));

  table().RemovePlusProfile(profile1.profile_id);
  std::vector<PlusAddressDataChange> remove_changes = {
      PlusAddressDataChange(PlusAddressDataChange::Type::kRemove, profile1)};
  EXPECT_CALL(observer, OnPlusAddressesChanged(remove_changes));
  service().OnWebDataChangedBySync(remove_changes);
  EXPECT_THAT(service().GetPlusProfiles(), UnorderedElementsAre(profile2));
  service().RemoveObserver(&observer);
}

class PlusAddressServiceDisabledTest : public PlusAddressServiceTest {
 protected:
  PlusAddressServiceDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(features::kPlusAddressesEnabled);
    InitService();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PlusAddressServiceDisabledTest, FeatureExplicitlyDisabled) {
  // `ShouldShowManualFallback` should return `false`, even if there's a
  // signed-in user.
  identity_env().MakeAccountAvailable("plus@plus.plus",
                                      {signin::ConsentLevel::kSignin});
  InitService();
  EXPECT_FALSE(service().ShouldShowManualFallback(
      url::Origin::Create(GURL("https://test.example")),
      /*is_off_the_record=*/false));
}

class PlusAddressServiceEnabledTest : public PlusAddressServiceTest {
 public:
  PlusAddressServiceEnabledTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPlusAddressesEnabled,
        {{features::kEnterprisePlusAddressServerUrl.name, "mattwashere"}});
    InitService();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(PlusAddressServiceEnabledTest, NoSignedInUser) {
  // Without a signed in user, the `ShouldShowManualFallback` should return
  // `false`.
  EXPECT_FALSE(service().ShouldShowManualFallback(kNoSubdomainOrigin,
                                                  /*is_off_the_record=*/false));
}

TEST_F(PlusAddressServiceEnabledTest, FullySupported) {
  // With a signed in user, the `ShouldShowManualFallback` function should
  // return `true`.
  identity_env().MakeAccountAvailable("plus@plus.plus",
                                      {signin::ConsentLevel::kSignin});
  InitService();
  EXPECT_TRUE(service().ShouldShowManualFallback(kNoSubdomainOrigin,
                                                 /*is_off_the_record=*/false));
}

// `ShouldShowManualFallback` returns false when `origin` is included on
// `kPlusAddressExcludedSites` and true otherwise.
TEST_F(PlusAddressServiceEnabledTest, ExcludedSitesAreNotSupported) {
  identity_env().MakeAccountAvailable("plus@plus.plus",
                                      {signin::ConsentLevel::kSignin});
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kPlusAddressesEnabled,
      {{features::kEnterprisePlusAddressServerUrl.name, "mattwashere"},
       {features::kPlusAddressExcludedSites.name,
        "exclude.co.th,forbidden.com"}});
  InitService();

  // Verify that url not on the excluded site continues to work.
  EXPECT_TRUE(service().ShouldShowManualFallback(
      url::Origin::Create(GURL("https://test.example")),
      /*is_off_the_record=*/false));

  // Sites on excluded list are not supported.
  EXPECT_FALSE(service().ShouldShowManualFallback(
      url::Origin::Create(GURL("https://www.forbidden.com")),
      /*is_off_the_record=*/false));
  EXPECT_FALSE(service().ShouldShowManualFallback(
      url::Origin::Create(GURL("https://www.exclude.co.th")),
      /*is_off_the_record=*/false));

  // Excluded site with different subdomain are also not supported.
  EXPECT_FALSE(service().ShouldShowManualFallback(
      url::Origin::Create(GURL("https://myaccount.forbidden.com")),
      /*is_off_the_record=*/false));
}

// `ShouldShowManualFallback` returns false when `origin` scheme is not http or
// https.
TEST_F(PlusAddressServiceEnabledTest, NonHTTPSchemesAreNotSupported) {
  identity_env().MakeAccountAvailable("plus@plus.plus",
                                      {signin::ConsentLevel::kSignin});
  InitService();
  EXPECT_TRUE(service().ShouldShowManualFallback(kNoSubdomainOrigin,
                                                 /*is_off_the_record=*/false));
  EXPECT_FALSE(service().ShouldShowManualFallback(
      url::Origin::Create(GURL("other://hello")),
      /*is_off_the_record=*/false));
}

// `ShouldShowManualFallback` returns false when `origin` is opaque.
TEST_F(PlusAddressServiceEnabledTest, OpaqueOriginIsNotSupported) {
  identity_env().MakeAccountAvailable("plus@plus.plus",
                                      {signin::ConsentLevel::kSignin});
  InitService();
  EXPECT_FALSE(service().ShouldShowManualFallback(url::Origin(), false));
}

TEST_F(PlusAddressServiceEnabledTest, OTRWithNoExistingAddress) {
  // With a signed in user, an off-the-record session, and no existing address,
  // the `ShouldShowManualFallback` function should return `false`.
  identity_env().MakeAccountAvailable("plus@plus.plus",
                                      {signin::ConsentLevel::kSignin});
  InitService();
  EXPECT_FALSE(service().ShouldShowManualFallback(kNoSubdomainOrigin,
                                                  /*is_off_the_record=*/true));
}

TEST_F(PlusAddressServiceEnabledTest, OTRWithExistingAddress) {
  // With a signed in user, an off-the-record session, and an existing address,
  // the `ShouldShowManualFallback` function should return `true`.
  identity_env().MakeAccountAvailable("plus@plus.plus",
                                      {signin::ConsentLevel::kSignin});
  InitService();

  const PlusProfile profile = test::CreatePlusProfile();
  service().SavePlusProfile(profile);
  EXPECT_TRUE(service().ShouldShowManualFallback(OriginFromFacet(profile.facet),
                                                 /*is_off_the_record=*/true));
}

TEST_F(PlusAddressServiceEnabledTest, GlobalSettingsToggleOff) {
  base::test::ScopedFeatureList feature_list{
      features::kPlusAddressGlobalToggle};
  identity_env().MakeAccountAvailable("plus@plus.plus",
                                      {signin::ConsentLevel::kSignin});
  InitService();
  setting_service().set_is_plus_addresses_enabled(false);
  EXPECT_FALSE(service().ShouldShowManualFallback(kNoSubdomainOrigin,
                                                  /*is_off_the_record=*/false));
}

TEST_F(PlusAddressServiceEnabledTest, SignedOutGetEmail) {
  EXPECT_EQ(service().GetPrimaryEmail(), std::nullopt);
}

TEST_F(PlusAddressServiceEnabledTest, SignedInGetEmail) {
  constexpr std::string_view expected_email = "plus@plus.plus";
  identity_env().MakeAccountAvailable(expected_email,
                                      {signin::ConsentLevel::kSignin});
  InitService();

  EXPECT_EQ(service().GetPrimaryEmail(), expected_email);
}

// Tests that PlusAddresses is "disabled" in the following states:
// - When a primary account is unset after login.
// - When a primary account's refresh token has an auth error.
//
// If PlusAddressService is "disabled" it should stop offering the feature,
// clear any local storage, and not issue network requests.
class PlusAddressServiceSignoutTest : public PlusAddressServiceTest {
 public:
  PlusAddressServiceSignoutTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPlusAddressesEnabled,
        {{features::kEnterprisePlusAddressServerUrl.name, "mattwashere"},
         {features::kEnterprisePlusAddressOAuthScope.name, "scope.example"}});
    secondary_account_ = identity_env().MakeAccountAvailable(
        "beta@plus.plus", {signin::ConsentLevel::kSignin});
    primary_account_ = identity_env().MakePrimaryAccountAvailable(
        "alpha@plus.plus", signin::ConsentLevel::kSignin);
    InitService();
  }

  const CoreAccountInfo& primary_account() const { return primary_account_; }
  const AccountInfo& secondary_account() const { return secondary_account_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  CoreAccountInfo primary_account_;
  AccountInfo secondary_account_;
};

// Doesn't run on ChromeOS since ClearPrimaryAccount() doesn't exist for it.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PlusAddressServiceSignoutTest, PrimaryAccountCleared_TogglesIsEnabled) {
  ASSERT_TRUE(service().IsEnabled());

  // Verify behaviors expected when service is enabled.
  const PlusProfile profile = test::CreatePlusProfile();
  const url::Origin origin = OriginFromFacet(profile.facet);
  service().SavePlusProfile(profile);
  EXPECT_TRUE(
      service().ShouldShowManualFallback(origin, /*is_off_the_record=*/false));
  EXPECT_TRUE(service().GetPlusAddress(profile.facet));
  EXPECT_EQ(service().GetPlusAddress(profile.facet).value(),
            profile.plus_address);
  EXPECT_TRUE(service().IsPlusAddress(profile.plus_address));

  identity_env().ClearPrimaryAccount();
  EXPECT_FALSE(service().IsEnabled());

  // Ensure that the local data is cleared on disabling.
  EXPECT_FALSE(service().ShouldShowManualFallback(origin,
                                                  /*is_off_the_record=*/false));
  EXPECT_FALSE(service().IsPlusAddress(profile.plus_address));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PlusAddressServiceSignoutTest,
       PrimaryRefreshTokenError_TogglesIsEnabled) {
  ASSERT_TRUE(service().IsEnabled());

  // Verify behaviors expected when service is enabled.
  const PlusProfile profile = test::CreatePlusProfile();
  const url::Origin origin = OriginFromFacet(profile.facet);
  service().SavePlusProfile(profile);
  EXPECT_TRUE(
      service().ShouldShowManualFallback(origin, /*is_off_the_record=*/false));
  EXPECT_TRUE(service().GetPlusAddress(profile.facet));
  EXPECT_EQ(service().GetPlusAddress(profile.facet).value(),
            profile.plus_address);
  EXPECT_TRUE(service().IsPlusAddress(profile.plus_address));

  // Setting to NONE doesn't disable the service.
  identity_env().UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account().account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
  EXPECT_TRUE(service().IsEnabled());

  // The PlusAddressService isn't disabled for secondary account auth errors.
  identity_env().UpdatePersistentErrorOfRefreshTokenForAccount(
      secondary_account().account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(service().IsEnabled());

  // Being in the "sync-paused" state results in this error.
  identity_env().UpdatePersistentErrorOfRefreshTokenForAccount(
      primary_account().account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_FALSE(service().IsEnabled());

  // Ensure that the local data is cleared on disabling.
  EXPECT_FALSE(
      service().ShouldShowManualFallback(origin, /*is_off_the_record=*/false));
  EXPECT_FALSE(service().IsPlusAddress(profile.plus_address));
}

// A test fixture with a `PlusAddressService` that is enabled to allow testing
// suggestion generation.
class PlusAddressSuggestionsTest : public PlusAddressServiceTest {
 public:
  PlusAddressSuggestionsTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kPlusAddressesEnabled,
        {{"server-url", "https://server.example"},
         {"oauth-scope", "scope.example"}});
    identity_env().MakePrimaryAccountAvailable("plus@plus.plus",
                                               signin::ConsentLevel::kSignin);
    identity_env().SetAutomaticIssueOfAccessTokens(true);
    InitService();
  }

 protected:
  static constexpr std::string_view kPlusAddressSuggestionMetric =
      "PlusAddresses.Suggestion.Events";

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that fill plus address suggestions are offered iff the value in the
// focused field matches the prefix of an existing plus address.
TEST_F(PlusAddressSuggestionsTest, SuggestionsForExistingPlusAddress) {
  base::HistogramTester histogram_tester;
  const PlusProfile profile = test::CreatePlusProfile();
  const url::Origin origin = OriginFromFacet(profile.facet);
  service().SavePlusProfile(profile);

  // We offer filling if the field is empty.
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kExistingPlusAddressSuggested, 1);

  // If the user types a letter and it matches the plus address (after
  // normalization), the plus address continues to be offered.
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"P",
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kExistingPlusAddressSuggested, 2);

  // If the value does not match the prefix of the plus address, nothing is
  // shown.
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"pp",
      AutofillSuggestionTriggerSource::kFormControlElementClicked, IsEmpty()));
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kExistingPlusAddressSuggested, 2);
}

// Tests that `GetSuggestions()` suggests plus profiles across eTLD+1s.
TEST_F(PlusAddressSuggestionsTest, SuggestionsForETLD) {
  const PlusProfile profile(/*profile_id=*/"123", "foo.com",
                            "plus+foo@plus.plus",
                            /*is_confirmed=*/true);
  service().SavePlusProfile(profile);
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      OriginFromFacet(profile.facet),
      /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      OriginFromFacet("asd.foo.com"),
      /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
}

// Tests that fill plus address suggestions regardless of whether there is
// already text in the field if the trigger source was manual fallback.
TEST_F(PlusAddressSuggestionsTest,
       SuggestionsForExistingPlusAddressWithManualFallback) {
  base::HistogramTester histogram_tester;
  const PlusProfile profile = test::CreatePlusProfile();
  const url::Origin origin = OriginFromFacet(profile.facet);
  service().SavePlusProfile(profile);

  // We offer filling if the field is empty.
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kExistingPlusAddressSuggested, 1);

  // We also offer filling if the field is not empty and the prefix does not
  // match the address.
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"pp",
      AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kExistingPlusAddressSuggested, 2);
}

// Tests that a create plus address suggestion is offered if there is no
// existing plus address for the domain and the field value is empty.
TEST_F(PlusAddressSuggestionsTest, SuggestionsForCreateNewPlusAddress) {
  base::HistogramTester histogram_tester;
  const auto origin = url::Origin::Create(GURL("https://foo.com"));

  // We offer creation if the field is empty.
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      IsSingleCreatePlusAddressSuggestion()));
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kCreateNewPlusAddressSuggested, 1);

  // If the field value is not empty, nothing is shown.
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"some text",
      AutofillSuggestionTriggerSource::kFormControlElementClicked, IsEmpty()));
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kCreateNewPlusAddressSuggested, 1);
}

// Tests that a create plus address suggestion is offered regardless of the
// field's value if there is no existing plus address for the domain and the
// trigger source is a manual fallback.
TEST_F(PlusAddressSuggestionsTest,
       SuggestionsForCreateNewPlusAddressWithManualFallback) {
  base::HistogramTester histogram_tester;
  const auto origin = url::Origin::Create(GURL("https://foo.com"));

  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses,
      IsSingleCreatePlusAddressSuggestion()));
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kCreateNewPlusAddressSuggested, 1);

  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"some text",
      AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses,
      IsSingleCreatePlusAddressSuggestion()));
  histogram_tester.ExpectUniqueSample(
      kPlusAddressSuggestionMetric,
      SuggestionEvent::kCreateNewPlusAddressSuggested, 2);
}

// Tests that no suggestions are returned when plus address are disabled.
TEST_F(PlusAddressSuggestionsTest, NoSuggestionsWhenDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPlusAddressesEnabled);

  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      url::Origin::Create(GURL("https://foo.com")),
      /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kFormControlElementClicked, IsEmpty()));
}

// Tests that the only password form on which create suggestions are offered on
// click is a signup form, but that filling suggestions are always offered.
TEST_F(PlusAddressSuggestionsTest, SuggestionsOnPasswordForms) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kPlusAddressOfferCreationOnSingleUsernameForms);

  const PlusProfile profile = test::CreatePlusProfile();
  const url::Origin origin = OriginFromFacet(profile.facet);
  auto get_suggestions_for_form_type = [&](PasswordFormType type,
                                           const auto& matcher) {
    return ExpectServiceToReturnSuggestions(
        origin,
        /*is_off_the_record=*/false, type,
        /*focused_field_value=*/u"",
        AutofillSuggestionTriggerSource::kFormControlElementClicked, matcher);
  };

  using enum PasswordFormType;
  EXPECT_TRUE(get_suggestions_for_form_type(kLoginForm, IsEmpty()));
  EXPECT_TRUE(get_suggestions_for_form_type(kChangePasswordForm, IsEmpty()));
  EXPECT_TRUE(get_suggestions_for_form_type(kResetPasswordForm, IsEmpty()));
  EXPECT_TRUE(get_suggestions_for_form_type(kSingleUsernameForm, IsEmpty()));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kSignupForm, IsSingleCreatePlusAddressSuggestion()));

  service().SavePlusProfile(profile);
  EXPECT_TRUE(get_suggestions_for_form_type(
      kLoginForm, IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kChangePasswordForm,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kResetPasswordForm,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kSingleUsernameForm,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kSignupForm, IsSingleFillPlusAddressSuggestion(profile.plus_address)));
}

// Tests that plus address creation is offered on signup forms and single
// username forms if `kPlusAddressOfferCreationOnSingleUsernameForms` is
// enabled.
TEST_F(PlusAddressSuggestionsTest,
       SuggestionsOnPasswordFormWithSingleUsernameCreationEnabled) {
  base::test::ScopedFeatureList feature_list{
      features::kPlusAddressOfferCreationOnSingleUsernameForms};

  const PlusProfile profile = test::CreatePlusProfile();
  const url::Origin origin = OriginFromFacet(profile.facet);
  auto get_suggestions_for_form_type = [&](PasswordFormType type,
                                           const auto& matcher) {
    return ExpectServiceToReturnSuggestions(
        origin,
        /*is_off_the_record=*/false, type,
        /*focused_field_value=*/u"",
        AutofillSuggestionTriggerSource::kFormControlElementClicked, matcher);
  };

  using enum PasswordFormType;
  EXPECT_TRUE(get_suggestions_for_form_type(kLoginForm, IsEmpty()));
  EXPECT_TRUE(get_suggestions_for_form_type(kChangePasswordForm, IsEmpty()));
  EXPECT_TRUE(get_suggestions_for_form_type(kResetPasswordForm, IsEmpty()));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kSingleUsernameForm, IsSingleCreatePlusAddressSuggestion()));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kSignupForm, IsSingleCreatePlusAddressSuggestion()));

  service().SavePlusProfile(profile);
  EXPECT_TRUE(get_suggestions_for_form_type(
      kLoginForm, IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kChangePasswordForm,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kResetPasswordForm,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kSingleUsernameForm,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kSignupForm, IsSingleFillPlusAddressSuggestion(profile.plus_address)));
}

// Tests that create suggestions are offered regardless of form type if the
// trigger source is a manual fallback.
TEST_F(PlusAddressSuggestionsTest,
       SuggestionsOnPasswordFormsWithManualFallbacks) {
  const PlusProfile profile = test::CreatePlusProfile();
  const url::Origin origin = OriginFromFacet(profile.facet);
  auto get_suggestions_for_form_type = [&](PasswordFormType type,
                                           const auto& matcher) {
    return ExpectServiceToReturnSuggestions(
        origin,
        /*is_off_the_record=*/false, type,
        /*focused_field_value=*/u"",
        AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses, matcher);
  };

  using enum PasswordFormType;
  EXPECT_TRUE(get_suggestions_for_form_type(
      kLoginForm, IsSingleCreatePlusAddressSuggestion()));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kChangePasswordForm, IsSingleCreatePlusAddressSuggestion()));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kResetPasswordForm, IsSingleCreatePlusAddressSuggestion()));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kSingleUsernameForm, IsSingleCreatePlusAddressSuggestion()));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kSignupForm, IsSingleCreatePlusAddressSuggestion()));

  service().SavePlusProfile(profile);
  EXPECT_TRUE(get_suggestions_for_form_type(
      kLoginForm, IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kChangePasswordForm,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kResetPasswordForm,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kSingleUsernameForm,
      IsSingleFillPlusAddressSuggestion(profile.plus_address)));
  EXPECT_TRUE(get_suggestions_for_form_type(
      kSignupForm, IsSingleFillPlusAddressSuggestion(profile.plus_address)));
}

// Tests the content of the "Manage plus addresses..." suggestion.
TEST_F(PlusAddressSuggestionsTest, GetManagePlusAddressSuggestion) {
  EXPECT_THAT(service().GetManagePlusAddressSuggestion(),
              EqualsSuggestion(SuggestionType::kManagePlusAddress,
                               l10n_util::GetStringUTF16(
                                   IDS_PLUS_ADDRESS_MANAGE_PLUS_ADDRESSES_TEXT),
                               Suggestion::Icon::kGoogleMonochrome));
}

class PlusAddressAffiliationsTest : public PlusAddressServiceTest {
 public:
  PlusAddressAffiliationsTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        // Enable features:
        {base::test::FeatureRefAndParams(
             features::kPlusAddressesEnabled,
             {{"server-url", "https://server.example"},
              {"oauth-scope", "scope.example"}}),
         base::test::FeatureRefAndParams(
             {GetSyncPlusAddressFeatureForTests(), {}}),
         base::test::FeatureRefAndParams(
             {features::kPlusAddressAffiliations, {}})},
        // Disable features:
        {});
    identity_env().MakePrimaryAccountAvailable("plus@plus.plus",
                                               signin::ConsentLevel::kSignin);
    identity_env().SetAutomaticIssueOfAccessTokens(true);
    InitService();
  }

  testing::AssertionResult ExpectServiceToReturnAffiliatedPlusProfiles(
      const url::Origin& origin,
      const auto& matcher) {
    base::MockCallback<PlusAddressService::GetPlusProfilesCallback> callback;
    int calls = 0;
    ON_CALL(callback, Run)
        .WillByDefault([&](std::vector<PlusProfile> plus_profiles) {
          EXPECT_THAT(plus_profiles, matcher);
          ++calls;
        });
    service().GetAffiliatedPlusProfiles(origin, callback.Get());
    return calls == 1
               ? testing::AssertionSuccess()
               : (testing::AssertionFailure() << "Error fetching suggestions.");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that affiliated PSL suggestions are returned. It also validates that
// entries in the PSL extensions list are respected.
TEST_F(PlusAddressAffiliationsTest, GetAffiliatedPSLSuggestions) {
  PlusProfile profile1 = PlusProfile(
      /*profile_id=*/"123",
      /*facet=*/FacetURI::FromCanonicalSpec("https://one.foo.example.com"),
      /*plus_address=*/"plus+one@plus.plus",
      /*is_confirmed=*/true);
  PlusProfile profile2 = PlusProfile(
      /*profile_id=*/"234",
      /*facet=*/FacetURI::FromCanonicalSpec("https://two.foo.example.com"),
      /*plus_address=*/"plus+foo@plus.plus",
      /*is_confirmed=*/true);
  PlusProfile profile3 = PlusProfile(
      /*profile_id=*/"345",
      /*facet=*/FacetURI::FromCanonicalSpec("https://bar.example.com"),
      /*plus_address=*/"plus+bar@plus.plus",
      /*is_confirmed=*/true);

  service().SavePlusProfile(profile1);
  service().SavePlusProfile(profile2);
  service().SavePlusProfile(profile3);
  ASSERT_THAT(service().GetPlusProfiles(),
              UnorderedElementsAre(profile1, profile2, profile3));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>{"example.com"}));

  // Empty affiliation group.
  affiliations::GroupedFacets group;
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  // Request the same URL as the `profile1.facet`.
  const url::Origin origin = url::Origin::Create(
      GURL(absl::get<FacetURI>(profile1.facet).canonical_spec()));

  // Note that `profile3` is not a PSL match due to the PSL extensions list.
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      UnorderedElementsAre(
          // Exact match.
          EqualsFillPlusAddressSuggestion(profile1.plus_address),
          // PSL match.
          EqualsFillPlusAddressSuggestion(profile2.plus_address))));
}

// Verifies that affiliated group suggestions are returned.
TEST_F(PlusAddressAffiliationsTest, GetAffiliatedGroupSuggestions) {
  PlusProfile group_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://group.affiliated.com"));

  // The user has the `group_profile` stored.
  service().SavePlusProfile(group_profile);
  ASSERT_THAT(service().GetPlusProfiles(), ElementsAre(group_profile));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));

  // Prepares the `group_profile` facet to be returned as part of the
  // affiliation group.
  affiliations::GroupedFacets group;
  group.facets.emplace_back(absl::get<FacetURI>(group_profile.facet));

  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      IsSingleFillPlusAddressSuggestion(group_profile.plus_address)));
}

// Tests that filling suggestions are returned even if they are affiliated
// matches and the profile is off the record.
TEST_F(PlusAddressAffiliationsTest,
       GetSuggestionsIsAffiliationAwareWhenOffTheRecord) {
  PlusProfile group_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://group.affiliated.com"));
  service().SavePlusProfile(group_profile);
  ASSERT_THAT(service().GetPlusProfiles(), ElementsAre(group_profile));

  ON_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillByDefault(RunOnceCallback<0>(std::vector<std::string>()));
  affiliations::GroupedFacets group;
  group.facets.emplace_back(absl::get<FacetURI>(group_profile.facet));
  ON_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillByDefault(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/true, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      IsSingleFillPlusAddressSuggestion(group_profile.plus_address)));
}

// Tests that no creation suggestion is offered when the profile is off the
// record.
TEST_F(PlusAddressAffiliationsTest,
       GetSuggestionsDoesNotOfferCreationWhenOffTheRecord) {
  ON_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillByDefault(RunOnceCallback<0>(std::vector<std::string>()));
  affiliations::GroupedFacets group;
  ON_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillByDefault(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/true, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kFormControlElementClicked, IsEmpty()));
}

// Tests that no creation suggestion is offered when the global toggle is off.
TEST_F(PlusAddressAffiliationsTest,
       GetSuggestionsDoesNotOfferCreationWhenToggleIsOff) {
  base::test::ScopedFeatureList feature_list{
      features::kPlusAddressGlobalToggle};
  ON_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillByDefault(RunOnceCallback<0>(std::vector<std::string>()));
  affiliations::GroupedFacets group;
  ON_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillByDefault(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));
  setting_service().set_is_plus_addresses_enabled(false);

  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kFormControlElementClicked, IsEmpty()));
}

// Tests that filling suggestions are returned even if they are affiliated
// matches and the global settings toggle is off.
TEST_F(PlusAddressAffiliationsTest,
       FillingSuggestionsAreOfferedWhenGlobalToggleIsOff) {
  base::test::ScopedFeatureList feature_list{
      features::kPlusAddressGlobalToggle};
  PlusProfile group_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://group.affiliated.com"));
  service().SavePlusProfile(group_profile);
  ASSERT_THAT(service().GetPlusProfiles(), ElementsAre(group_profile));

  ON_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillByDefault(RunOnceCallback<0>(std::vector<std::string>()));
  affiliations::GroupedFacets group;
  group.facets.emplace_back(absl::get<FacetURI>(group_profile.facet));
  ON_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillByDefault(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));
  setting_service().set_is_plus_addresses_enabled(false);

  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin, /*is_off_the_record=*/true, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      IsSingleFillPlusAddressSuggestion(group_profile.plus_address)));
}

// Verifies that no affiliated suggestions are returned when there are no
// matches. Instead, the creation chip is offered.
TEST_F(PlusAddressAffiliationsTest, GetEmptyAffiliatedSuggestionMatches) {
  PlusProfile stored_profile1 = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://foo.com"));
  PlusProfile stored_profile2 = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://bar.com"));

  service().SavePlusProfile(stored_profile1);
  service().SavePlusProfile(stored_profile2);
  ASSERT_THAT(service().GetPlusProfiles(),
              UnorderedElementsAre(stored_profile1, stored_profile2));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));

  affiliations::GroupedFacets group;
  group.facets.emplace_back(
      FacetURI::FromCanonicalSpec("https://group.affiliated.com"));

  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  const url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  EXPECT_TRUE(ExpectServiceToReturnSuggestions(
      origin,
      /*is_off_the_record=*/false, PasswordFormType::kNoPasswordForm,
      /*focused_field_value=*/u"",
      AutofillSuggestionTriggerSource::kFormControlElementClicked,
      // There are no PLS, group or exact matches.
      IsSingleCreatePlusAddressSuggestion()));
}

// Verifies that affiliated plus profiles are returned.
TEST_F(PlusAddressAffiliationsTest, GetAffiliatedPSLProfiles) {
  PlusProfile profile1 = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://one.foo.example.com"));
  PlusProfile profile2 = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://two.foo.example.com"));
  PlusProfile profile3 = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://bar.example.com"));

  service().SavePlusProfile(profile1);
  service().SavePlusProfile(profile2);
  service().SavePlusProfile(profile3);
  ASSERT_THAT(service().GetPlusProfiles(),
              UnorderedElementsAre(profile1, profile2, profile3));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>{"example.com"}));

  // Empty affiliation group.
  affiliations::GroupedFacets group;
  EXPECT_CALL(*mock_affiliation_service(), GetGroupingInfo)
      .WillOnce(
          RunOnceCallback<1>(std::vector<affiliations::GroupedFacets>{group}));

  // Request the same URL as the `profile1.facet`.
  const url::Origin origin = url::Origin::Create(
      GURL(absl::get<FacetURI>(profile1.facet).canonical_spec()));

  // Note that `profile3` is not a PSL match due to the PSL extensions list.
  ExpectServiceToReturnAffiliatedPlusProfiles(
      origin, UnorderedElementsAre(profile1, profile2));
}

// Verifies that the service returns profiles from affiliated domains even if
// the requested domain doesn't have an affiliated plus address.
TEST_F(PlusAddressAffiliationsTest,
       AffiliatedProfilesForDomainWithNoPlusAddresses) {
  PlusProfile group_profile = test::CreatePlusProfileWithFacet(
      FacetURI::FromCanonicalSpec("https://group.affiliated.com"));

  service().SavePlusProfile(group_profile);
  ASSERT_THAT(service().GetPlusProfiles(), UnorderedElementsAre(group_profile));

  EXPECT_CALL(*mock_affiliation_service(), GetPSLExtensions)
      .WillOnce(RunOnceCallback<0>(std::vector<std::string>()));

  // Prepares the `group_profile` facet to be returned as part of the
  // affiliation group.
  affiliations::GroupedFacets group;
  group.facets.emplace_back(absl::get<FacetURI>(group_profile.facet));

  const url::Origin origin =
      url::Origin::Create(GURL("https://bar.example.com"));

  ExpectServiceToReturnAffiliatedPlusProfiles(
      origin, UnorderedElementsAre(group_profile));
}

}  // namespace plus_addresses
