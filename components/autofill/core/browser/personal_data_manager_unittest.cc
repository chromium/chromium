// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class PersonalDataManagerHelper : public PersonalDataManagerTestBase {
 protected:
  PersonalDataManagerHelper() = default;

  virtual ~PersonalDataManagerHelper() {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_.reset();
  }

  void ResetPersonalDataManager(bool use_sync_transport_mode = false) {
    if (personal_data_)
      personal_data_->Shutdown();
    personal_data_ = std::make_unique<PersonalDataManager>("EN", "US");
    PersonalDataManagerTestBase::ResetPersonalDataManager(
        use_sync_transport_mode, personal_data_.get());
  }

  bool TurnOnSyncFeature() {
    return PersonalDataManagerTestBase::TurnOnSyncFeature(personal_data_.get());
  }

  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    PersonalDataChangedWaiter waiter(*personal_data_);
    personal_data_->AddProfile(profile);
    std::move(waiter).Wait();
  }

  std::unique_ptr<PersonalDataManager> personal_data_;
};

class PersonalDataManagerTest : public PersonalDataManagerHelper,
                                public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager();
  }
  void TearDown() override { TearDownTest(); }
};

class PersonalDataManagerSyncTransportModeTest
    : public PersonalDataManagerHelper,
      public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager(
        /*use_sync_transport_mode=*/true);
  }
  void TearDown() override { TearDownTest(); }
};

// Tests that `GetProfilesForSettings()` orders by descending modification
// dates.
// TODO(crbug.com/1420547): The modification date is set in AutofillTable.
// Setting it on the test profiles directly doesn't suffice.
TEST_F(PersonalDataManagerTest, GetProfilesForSettings) {
  TestAutofillClock test_clock;

  AutofillProfile kAccountProfile = test::GetFullProfile();
  kAccountProfile.set_source_for_testing(AutofillProfile::Source::kAccount);
  AddProfileToPersonalDataManager(kAccountProfile);

  AutofillProfile kLocalOrSyncableProfile = test::GetFullProfile2();
  kLocalOrSyncableProfile.set_source_for_testing(
      AutofillProfile::Source::kLocalOrSyncable);
  test_clock.Advance(base::Minutes(123));
  AddProfileToPersonalDataManager(kLocalOrSyncableProfile);

  EXPECT_THAT(personal_data_->GetProfilesForSettings(),
              testing::ElementsAre(testing::Pointee(kLocalOrSyncableProfile),
                                   testing::Pointee(kAccountProfile)));
}

TEST_F(PersonalDataManagerTest, SaveCardLocallyIfNewWithNewCard) {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Sunraku Emul",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");

  EXPECT_EQ(0U, personal_data_->GetCreditCards().size());

  // Add the credit card to the database.
  bool is_saved = personal_data_->SaveCardLocallyIfNew(credit_card);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Expect that the credit card was saved.
  EXPECT_TRUE(is_saved);
  std::vector<CreditCard> saved_credit_cards;
  for (auto* result : personal_data_->GetCreditCards()) {
    saved_credit_cards.push_back(*result);
  }

  EXPECT_THAT(saved_credit_cards, testing::ElementsAre(credit_card));
}

TEST_F(PersonalDataManagerTest, SaveCardLocallyIfNewWithExistingCard) {
  const char* credit_card_number = "4111 1111 1111 1111" /* Visa */;
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Sunraku Emul", credit_card_number,
                          "01", "2999", "");

  // Add the credit card to the database.
  personal_data_->AddCreditCard(credit_card);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  // Create a new credit card with the same card number but different detailed
  // information.
  CreditCard similar_credit_card(
      base::Uuid::GenerateRandomV4().AsLowercaseString(), kSettingsOrigin);
  test::SetCreditCardInfo(&similar_credit_card, "Sunraku Emul",
                          credit_card_number, "02", "3999",
                          "Different billing address");
  // Try to add the similar credit card to the database.
  bool is_saved = personal_data_->SaveCardLocallyIfNew(similar_credit_card);

  // Expect that the saved credit card was not updated.
  EXPECT_FALSE(is_saved);
  std::vector<CreditCard> saved_credit_cards;
  for (auto* result : personal_data_->GetCreditCards()) {
    saved_credit_cards.push_back(*result);
  }

  EXPECT_THAT(saved_credit_cards, testing::ElementsAre(credit_card));
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeIsCached) {
  // The return value should always be some country code, no matter what.
  std::string default_country =
      personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(2U, default_country.size());

  AutofillProfile profile = test::GetFullProfile();
  AddProfileToPersonalDataManager(profile);

  // The value is cached and doesn't change even after adding an address.
  EXPECT_EQ(default_country,
            personal_data_->GetDefaultCountryCodeForNewAddress());

  // Disabling Autofill blows away this cache and shouldn't account for Autofill
  // profiles.
  prefs::SetAutofillProfileEnabled(prefs_.get(), false);
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(default_country,
            personal_data_->GetDefaultCountryCodeForNewAddress());

  // Enabling Autofill blows away the cached value and should reflect the new
  // value (accounting for profiles).
  prefs::SetAutofillProfileEnabled(prefs_.get(), true);
  EXPECT_EQ(base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY)),
            personal_data_->GetDefaultCountryCodeForNewAddress());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeComesFromProfiles) {
  AutofillProfile canadian_profile = test::GetFullCanadianProfile();
  ASSERT_EQ(canadian_profile.GetRawInfo(ADDRESS_HOME_COUNTRY), u"CA");
  AddProfileToPersonalDataManager(canadian_profile);
  ResetPersonalDataManager();
  EXPECT_EQ("CA", personal_data_->GetDefaultCountryCodeForNewAddress());

  // Multiple profiles cast votes.
  AutofillProfile us_profile1 = test::GetFullProfile();
  AutofillProfile us_profile2 = test::GetFullProfile2();
  ASSERT_EQ(us_profile1.GetRawInfo(ADDRESS_HOME_COUNTRY), u"US");
  ASSERT_EQ(us_profile2.GetRawInfo(ADDRESS_HOME_COUNTRY), u"US");
  AddProfileToPersonalDataManager(us_profile1);
  AddProfileToPersonalDataManager(us_profile2);
  ResetPersonalDataManager();
  EXPECT_EQ("US", personal_data_->GetDefaultCountryCodeForNewAddress());
}

TEST_F(PersonalDataManagerTest, DefaultCountryCodeComesFromVariations) {
  const std::string expected_country_code = "DE";
  const std::string unexpected_country_code = "FR";

  // Normally, the variation country code is passed to the constructor.
  personal_data_->set_variations_country_code_for_testing(
      expected_country_code);

  // Since there are no profiles set, the country code supplied by variations
  // should be used get get a default country code.
  ASSERT_EQ(0u, personal_data_->GetProfiles().size());
  std::string actual_country_code =
      personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(expected_country_code, actual_country_code);

  // Set a new country code.
  // The default country code retrieved before should have been cached.
  personal_data_->set_variations_country_code_for_testing(
      unexpected_country_code);
  actual_country_code = personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(expected_country_code, actual_country_code);
}

// Test that profiles are not shown if |kAutofillProfileEnabled| is set to
// |false|.
TEST_F(PersonalDataManagerTest, GetProfilesToSuggest_ProfileAutofillDisabled) {
  const std::string kServerAddressId("server_address1");
  ASSERT_TRUE(TurnOnSyncFeature());

  // Add two different profiles, a local and a server one.
  AutofillProfile local_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(local_profile);

  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(prefs_.get(), false);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Check that profiles were saved.
  const size_t expected_profiles = 1;
  EXPECT_EQ(expected_profiles, personal_data_->GetProfiles().size());
  // Expect no autofilled values or suggestions.
  EXPECT_EQ(0U, personal_data_->GetProfilesToSuggest().size());
}

// Test that local and server profiles are not loaded into memory on start-up if
// |kAutofillProfileEnabled| is set to |false|.
TEST_F(PersonalDataManagerTest,
       GetProfilesToSuggest_NoProfilesLoadedIfDisabled) {
  const std::string kServerAddressId("server_address1");
  ASSERT_TRUE(TurnOnSyncFeature());

  // Add two different profiles, a local and a server one.
  AutofillProfile local_profile(
      i18n_model_definition::kLegacyHierarchyCountryCode);
  test::SetProfileInfo(&local_profile, "Josephine", "Alicia", "Saenz",
                       "joewayne@me.xyz", "Fox", "1212 Center.", "Bld. 5",
                       "Orlando", "FL", "32801", "US", "19482937549");
  AddProfileToPersonalDataManager(local_profile);

  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Expect that all profiles are suggested.
  const size_t expected_profiles = 1;
  EXPECT_EQ(expected_profiles, personal_data_->GetProfiles().size());
  EXPECT_EQ(expected_profiles, personal_data_->GetProfilesToSuggest().size());

  // Disable Profile autofill.
  prefs::SetAutofillProfileEnabled(prefs_.get(), false);
  // Reload the database.
  ResetPersonalDataManager();

  // Expect no profile values or suggestions were loaded.
  EXPECT_EQ(0U, personal_data_->GetProfilesToSuggest().size());
}

// Test that profiles are not added if `kAutofillProfileEnabled` is set to
// false.
TEST_F(PersonalDataManagerTest,
       GetProfilesToSuggest_NoProfilesAddedIfDisabled) {
  prefs::SetAutofillProfileEnabled(prefs_.get(), false);
  AddProfileToPersonalDataManager(test::GetFullProfile());
  EXPECT_TRUE(personal_data_->GetProfiles().empty());
}

TEST_F(PersonalDataManagerTest, ClearAllLocalData) {
  // Add some local data.
  AddProfileToPersonalDataManager(test::GetFullProfile());
  personal_data_->AddCreditCard(test::GetCreditCard());
  personal_data_->Refresh();

  // The card and profile should be there.
  ResetPersonalDataManager();
  EXPECT_FALSE(personal_data_->GetCreditCards().empty());
  EXPECT_FALSE(personal_data_->GetProfiles().empty());

  personal_data_->ClearAllLocalData();

  // Reload the database, everything should be gone.
  ResetPersonalDataManager();
  EXPECT_TRUE(personal_data_->GetCreditCards().empty());
  EXPECT_TRUE(personal_data_->GetProfiles().empty());
}

// Sync Transport mode is only for Win, Mac, and Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(PersonalDataManagerSyncTransportModeTest,
       AutofillSyncToggleAvailableInTransportMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{syncer::
                                kSyncEnableContactInfoDataTypeInTransportMode,
                            ::switches::kExplicitBrowserSigninUIOnDesktop},
      /*disabled_features=*/{});

  prefs_->SetBoolean(::prefs::kExplicitBrowserSignin, true);
  EXPECT_TRUE(personal_data_->IsAutofillSyncToggleAvailable());

  prefs_->SetBoolean(::prefs::kExplicitBrowserSignin, false);
  EXPECT_FALSE(personal_data_->IsAutofillSyncToggleAvailable());
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

TEST_F(PersonalDataManagerTest, GetAccountInfoForPaymentsServer) {
  // Make the IdentityManager return a non-empty AccountInfo when
  // GetPrimaryAccountInfo() is called.
  std::string sync_account_email =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
          .email;
  ASSERT_FALSE(sync_account_email.empty());

  // Make the sync service returns consistent AccountInfo when GetAccountInfo()
  // is called.
  ASSERT_EQ(sync_service_.GetAccountInfo().email, sync_account_email);

  // The Active Sync AccountInfo should be returned.
  EXPECT_EQ(sync_account_email,
            personal_data_->GetAccountInfoForPaymentsServer().email);
}

TEST_F(PersonalDataManagerTest, OnAccountsCookieDeletedByUserAction) {
  // Set up some sync transport opt-ins in the prefs.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), CoreAccountId::FromGaiaId("account1"), true);
  EXPECT_FALSE(prefs_->GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Simulate that the cookies get cleared by the user.
  personal_data_->OnAccountsCookieDeletedByUserAction();

  // Make sure the pref is now empty.
  EXPECT_TRUE(prefs_->GetDict(prefs::kAutofillSyncTransportOptIn).empty());
}


TEST_F(PersonalDataManagerTest, ClearFullBrowsingHistory) {
  GURL domain("https://www.block.me/index.html");
  AddressDataManager& adm = personal_data_->address_data_manager();

  adm.AddStrikeToBlockNewProfileImportForDomain(domain);
  adm.AddStrikeToBlockNewProfileImportForDomain(domain);
  adm.AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_TRUE(adm.IsNewProfileImportBlockedForDomain(domain));

  history::DeletionInfo deletion_info = history::DeletionInfo::ForAllHistory();

  personal_data_->OnHistoryDeletions(/*history_service=*/nullptr,
                                     deletion_info);

  EXPECT_FALSE(adm.IsNewProfileImportBlockedForDomain(domain));
}

TEST_F(PersonalDataManagerTest, ClearUrlsFromBrowsingHistory) {
  GURL first_url("https://www.block.me/index.html");
  GURL second_url("https://www.block.too/index.html");

  // Add strikes to block both domains.
  AddressDataManager& adm = personal_data_->address_data_manager();
  adm.AddStrikeToBlockNewProfileImportForDomain(first_url);
  adm.AddStrikeToBlockNewProfileImportForDomain(first_url);
  adm.AddStrikeToBlockNewProfileImportForDomain(first_url);
  EXPECT_TRUE(adm.IsNewProfileImportBlockedForDomain(first_url));

  adm.AddStrikeToBlockNewProfileImportForDomain(second_url);
  adm.AddStrikeToBlockNewProfileImportForDomain(second_url);
  adm.AddStrikeToBlockNewProfileImportForDomain(second_url);
  EXPECT_TRUE(adm.IsNewProfileImportBlockedForDomain(second_url));

  history::URLRows deleted_urls = {history::URLRow(first_url)};

  history::DeletionInfo deletion_info =
      history::DeletionInfo::ForUrls(deleted_urls, {});

  personal_data_->OnHistoryDeletions(/*history_service=*/nullptr,
                                     deletion_info);

  // The strikes for `domain` should be deleted, but the strikes for
  // `another_domain` should not.
  EXPECT_FALSE(adm.IsNewProfileImportBlockedForDomain(first_url));
  EXPECT_TRUE(adm.IsNewProfileImportBlockedForDomain(second_url));
}

TEST_F(PersonalDataManagerTest, ClearUrlsFromBrowsingHistoryInTimeRange) {
  GURL first_url("https://www.block.me/index.html");
  GURL second_url("https://www.block.too/index.html");

  TestAutofillClock test_clock;

  // Add strikes to block both domains.
  AddressDataManager& adm = personal_data_->address_data_manager();
  adm.AddStrikeToBlockNewProfileImportForDomain(first_url);
  adm.AddStrikeToBlockNewProfileImportForDomain(first_url);
  adm.AddStrikeToBlockNewProfileImportForDomain(first_url);
  adm.AddStrikeToBlockNewProfileImportForDomain(second_url);
  adm.AddStrikeToBlockNewProfileImportForDomain(second_url);
  EXPECT_TRUE(adm.IsNewProfileImportBlockedForDomain(first_url));

  test_clock.Advance(base::Hours(1));
  base::Time end_of_deletion = AutofillClock::Now();
  test_clock.Advance(base::Hours(1));

  adm.AddStrikeToBlockNewProfileImportForDomain(second_url);
  EXPECT_TRUE(adm.IsNewProfileImportBlockedForDomain(second_url));

  history::URLRows deleted_urls = {history::URLRow(first_url),
                                   history::URLRow(second_url)};

  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(base::Time::Min(), end_of_deletion), false,
      deleted_urls, {},
      std::make_optional<std::set<GURL>>({first_url, second_url}));

  personal_data_->OnHistoryDeletions(/*history_service=*/nullptr,
                                     deletion_info);

  // The strikes for `first_url` should be deleted because the strikes have been
  // added within the deletion time range.
  EXPECT_FALSE(adm.IsNewProfileImportBlockedForDomain(first_url));
  // The last strike for 'second_url' was collected after the deletion time
  // range and therefore, the blocking should prevail.
  EXPECT_TRUE(adm.IsNewProfileImportBlockedForDomain(second_url));
}

// On mobile, no dedicated opt-in is required for WalletSyncTransport - the
// user is always considered opted-in and thus this test doesn't make sense.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerSyncTransportModeTest, OnUserAcceptedUpstreamOffer) {
  ///////////////////////////////////////////////////////////
  // kSignedInAndWalletSyncTransportEnabled
  ///////////////////////////////////////////////////////////
  // Make sure a primary account with no sync consent is available so
  // AUTOFILL_WALLET_DATA can run in sync-transport mode.
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  CoreAccountInfo active_info =
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);
  sync_service_.SetAccountInfo(active_info);
  sync_service_.SetHasSyncConsent(false);

  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill,
                 syncer::UserSelectableType::kPayments});
  // Make sure there are no opt-ins recorded yet.
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  // Account wallet storage only makes sense together with support for
  // unconsented primary accounts, i.e. on Win/Mac/Linux.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(
      !sync_service_.IsSyncFeatureEnabled() &&
      sync_service_.GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA));

  // Make sure an opt-in gets recorded if the user accepted an Upstream offer.
  personal_data_->OnUserAcceptedUpstreamOffer();
  EXPECT_TRUE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                      active_info.account_id));

  // Clear the prefs.
  prefs::ClearSyncTransportOptIns(prefs_.get());
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  ///////////////////////////////////////////////////////////
  // kSignedIn
  ///////////////////////////////////////////////////////////
  // Disable the wallet data type. kSignedInAndWalletSyncTransportEnabled
  // shouldn't be available.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_TRUE(!sync_service_.GetAccountInfo().IsEmpty());

  // Make sure an opt-in does not get recorded even if the user accepted an
  // Upstream offer.
  personal_data_->OnUserAcceptedUpstreamOffer();
  EXPECT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  // Clear the prefs.
  prefs::ClearSyncTransportOptIns(prefs_.get());
  ASSERT_FALSE(prefs::IsUserOptedInWalletSyncTransport(prefs_.get(),
                                                       active_info.account_id));

  ///////////////////////////////////////////////////////////
  // kSignedOut
  ///////////////////////////////////////////////////////////
  identity_test_env_.ClearPrimaryAccount();
  sync_service_.SetAccountInfo(CoreAccountInfo());
  sync_service_.SetHasSyncConsent(false);
  {
    EXPECT_TRUE(sync_service_.GetAccountInfo().IsEmpty());

    // Make sure an opt-in does not get recorded even if the user accepted an
    // Upstream offer.
    personal_data_->OnUserAcceptedUpstreamOffer();
    EXPECT_FALSE(prefs::IsUserOptedInWalletSyncTransport(
        prefs_.get(), active_info.account_id));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  ///////////////////////////////////////////////////////////
  // kSignedInAndSyncFeature
  ///////////////////////////////////////////////////////////
  identity_test_env_.MakePrimaryAccountAvailable(active_info.email,
                                                 signin::ConsentLevel::kSync);
  sync_service_.SetAccountInfo(active_info);
  sync_service_.SetHasSyncConsent(true);
  {
    EXPECT_TRUE(sync_service_.IsSyncFeatureEnabled());

    // Make sure an opt-in does not get recorded even if the user accepted an
    // Upstream offer.
    personal_data_->OnUserAcceptedUpstreamOffer();
    EXPECT_FALSE(prefs::IsUserOptedInWalletSyncTransport(
        prefs_.get(), active_info.account_id));
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(PersonalDataManagerTest, IsEligibleForAddressAccountStorage) {
  // All data types are running by default.
  EXPECT_TRUE(personal_data_->IsEligibleForAddressAccountStorage());

  // No Sync, no account storage.
  personal_data_->SetSyncServiceForTest(nullptr);
  EXPECT_FALSE(personal_data_->IsEligibleForAddressAccountStorage());
}

TEST_F(PersonalDataManagerTest, IsCountryEligibleForAccountStorage) {
  EXPECT_TRUE(personal_data_->IsCountryEligibleForAccountStorage("AT"));
  EXPECT_FALSE(personal_data_->IsCountryEligibleForAccountStorage("IR"));
}

TEST_F(PersonalDataManagerTest, AccountStatusSyncRetrieval) {
  EXPECT_NE(personal_data_->GetAccountStatusForTesting(), std::nullopt);

  // Login with a non-enterprise account (the status is expected to be available
  // immediately, with no async calls).
  AccountInfo account = identity_test_env_.MakeAccountAvailable("ab@gmail.com");
  sync_service_.SetAccountInfo(account);
  sync_service_.FireStateChanged();
  EXPECT_EQ(personal_data_->GetAccountStatusForTesting(),
            signin::AccountManagedStatusFinder::Outcome::kNonEnterprise);

  personal_data_->SetSyncServiceForTest(nullptr);
  EXPECT_EQ(personal_data_->GetAccountStatusForTesting(), std::nullopt);
}

TEST_F(PersonalDataManagerTest, ChangeCallbackIsTriggeredOnAddedProfile) {
  ::testing::StrictMock<base::MockOnceClosure> callback;
  EXPECT_CALL(callback, Run);

  personal_data_->AddChangeCallback(callback.Get());
  AddProfileToPersonalDataManager(test::GetFullProfile());
}

}  // namespace autofill
