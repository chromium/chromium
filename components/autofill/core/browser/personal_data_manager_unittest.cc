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
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/browser/profile_token_quality_test_api.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
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

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace autofill {

namespace {

constexpr char kPrimaryAccountEmail[] = "syncuser@example.com";

template <typename T>
bool CompareElements(T* a, T* b) {
  return a->Compare(*b) < 0;
}

template <typename T>
bool ElementsEqual(T* a, T* b) {
  return a->Compare(*b) == 0;
}

// Verifies that two vectors have the same elements (according to T::Compare)
// while ignoring order. This is useful because multiple profiles or credit
// cards that are added to the SQLite DB within the same second will be returned
// in GUID (aka random) order.
template <typename T>
void ExpectSameElements(const std::vector<T*>& expectations,
                        const std::vector<T*>& results) {
  ASSERT_EQ(expectations.size(), results.size());

  std::vector<T*> expectations_copy = expectations;
  std::sort(expectations_copy.begin(), expectations_copy.end(),
            CompareElements<T>);
  std::vector<T*> results_copy = results;
  std::sort(results_copy.begin(), results_copy.end(), CompareElements<T>);

  EXPECT_EQ(
      base::ranges::mismatch(results_copy, expectations_copy, ElementsEqual<T>)
          .first,
      results_copy.end());
}

}  // anonymous namespace

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

  // Adds three local cards to the |personal_data_|. The three cards are
  // different: two are from different companies and the third doesn't have a
  // number. All three have different owners and credit card number. This allows
  // to test the suggestions based on name as well as on credit card number.
  void SetUpReferenceLocalCreditCards() {
    ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

    CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                            test::kEmptyOrigin);
    test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                            "378282246310005" /* American Express */, "04",
                            "2999", "1");
    credit_card0.set_use_count(3);
    credit_card0.set_use_date(AutofillClock::Now() - base::Days(1));
    personal_data_->AddCreditCard(credit_card0);

    CreditCard credit_card1("1141084B-72D7-4B73-90CF-3D6AC154673B",
                            test::kEmptyOrigin);
    credit_card1.set_use_count(300);
    credit_card1.set_use_date(AutofillClock::Now() - base::Days(10));
    test::SetCreditCardInfo(&credit_card1, "John Dillinger",
                            "4234567890123456" /* Visa */, "01", "2999", "1");
    personal_data_->AddCreditCard(credit_card1);

    CreditCard credit_card2("002149C1-EE28-4213-A3B9-DA243FFF021B",
                            test::kEmptyOrigin);
    credit_card2.set_use_count(1);
    credit_card2.set_use_date(AutofillClock::Now() - base::Days(1));
    test::SetCreditCardInfo(&credit_card2, "Bonnie Parker",
                            "5105105105105100" /* Mastercard */, "12", "2999",
                            "1");
    PersonalDataChangedWaiter waiter(*personal_data_);
    personal_data_->AddCreditCard(credit_card2);
    std::move(waiter).Wait();
    ASSERT_EQ(3U, personal_data_->GetCreditCards().size());
  }

  // Add 2 credit cards. One local, one masked.
  void SetUpTwoCardTypes() {
    EXPECT_EQ(0U, personal_data_->GetCreditCards().size());
    CreditCard masked_server_card;
    test::SetCreditCardInfo(&masked_server_card, "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    masked_server_card.set_guid("00000000-0000-0000-0000-000000000007");
    masked_server_card.set_record_type(CreditCard::RecordType::kFullServerCard);
    masked_server_card.set_server_id("masked_id");
    masked_server_card.set_use_count(15);
    {
      PersonalDataChangedWaiter waiter(*personal_data_);
      // TODO(crbug.com/1497734): Switch to an appropriate setter for masked
      // cards, as full cards have been removed.
      personal_data_->AddFullServerCreditCardForTesting(masked_server_card);
      std::move(waiter).Wait();
    }
    ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

    CreditCard local_card;
    test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                            "4234567890123463",  // Visa
                            "08", "2999", "1");
    local_card.set_guid("00000000-0000-0000-0000-000000000009");
    local_card.set_record_type(CreditCard::RecordType::kLocalCard);
    local_card.set_use_count(5);
    {
      PersonalDataChangedWaiter waiter(*personal_data_);
      personal_data_->AddCreditCard(local_card);
      std::move(waiter).Wait();
    }
    ASSERT_EQ(2U, personal_data_->GetCreditCards().size());
  }

  PaymentsAutofillTable* GetServerDataTable() {
    return personal_data_->IsSyncFeatureEnabledForPaymentsServerMetrics()
               ? profile_autofill_table_.get()
               : account_autofill_table_.get();
  }

  void AddProfileToPersonalDataManager(const AutofillProfile& profile) {
    PersonalDataChangedWaiter waiter(*personal_data_);
    personal_data_->AddProfile(profile);
    std::move(waiter).Wait();
  }

  void SetServerCards(const std::vector<CreditCard>& server_cards) {
    test::SetServerCreditCards(GetServerDataTable(), server_cards);
  }

  void AddOfferDataForTest(AutofillOfferData offer_data) {
    personal_data_->AddOfferDataForTest(
        std::make_unique<AutofillOfferData>(offer_data));
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

#if BUILDFLAG(IS_ANDROID)
TEST_F(PersonalDataManagerTest,
       AutofillPaymentMethodsMandatoryReauthAlwaysEnabledOnAutomotive) {
  if (!base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should only run on automotive.";
  }

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);

  EXPECT_TRUE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());

  EXPECT_CHECK_DEATH_WITH(
      { personal_data_->SetPaymentMethodsMandatoryReauthEnabled(false); },
      "This feature should not be able to be turned off on automotive "
      "devices.");

  EXPECT_TRUE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
// Test that setting the `kAutofillEnablePaymentsMandatoryReauth` pref works
// correctly.
TEST_F(PersonalDataManagerTest, AutofillPaymentMethodsMandatoryReauthEnabled) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
  EXPECT_FALSE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());

  personal_data_->SetPaymentMethodsMandatoryReauthEnabled(true);

  EXPECT_TRUE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());

  personal_data_->SetPaymentMethodsMandatoryReauthEnabled(false);

  EXPECT_FALSE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());
}

// Test that setting the `kAutofillEnablePaymentsMandatoryReauth` does not
// enable the feature when the flag is off.
TEST_F(PersonalDataManagerTest,
       AutofillPaymentMethodsMandatoryReauthEnabled_FlagOff) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
  EXPECT_FALSE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());

  personal_data_->SetPaymentMethodsMandatoryReauthEnabled(true);

  EXPECT_FALSE(personal_data_->IsPaymentMethodsMandatoryReauthEnabled());
}

// Test that
// `PersonalDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo()`
// only returns that we should show the promo when we are below the max counter
// limit for showing the promo.
TEST_F(
    PersonalDataManagerTest,
    ShouldShowPaymentMethodsMandatoryReauthPromo_MaxValueForPromoShownCounterReached) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
  base::HistogramTester histogram_tester;
  for (int i = 0; i < prefs::kMaxValueForMandatoryReauthPromoShownCounter;
       i++) {
    // This also verifies that ShouldShowPaymentMethodsMandatoryReauthPromo()
    // works as expected when below the max cap.
    EXPECT_TRUE(personal_data_->ShouldShowPaymentMethodsMandatoryReauthPromo());
    personal_data_->IncrementPaymentMethodsMandatoryReauthPromoShownCounter();
  }

  EXPECT_FALSE(personal_data_->ShouldShowPaymentMethodsMandatoryReauthPromo());
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.MandatoryReauth.CheckoutFlow."
      "ReauthOfferOptInDecision2",
      autofill_metrics::MandatoryReauthOfferOptInDecision::
          kBlockedByStrikeDatabase,
      1);
}

// Test that
// `PersonalDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo()`
// returns that we should not show the promo if the user already opted in.
TEST_F(PersonalDataManagerTest,
       ShouldShowPaymentMethodsMandatoryReauthPromo_UserOptedInAlready) {
#if BUILDFLAG(IS_ANDROID)
  // Opt-in prompts are not shown on automotive as mandatory reauth is always
  // enabled.
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
  base::HistogramTester histogram_tester;
  // Simulate user is already opted in.
  personal_data_->SetPaymentMethodsMandatoryReauthEnabled(true);

  EXPECT_FALSE(personal_data_->ShouldShowPaymentMethodsMandatoryReauthPromo());
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.MandatoryReauth.CheckoutFlow."
      "ReauthOfferOptInDecision2",
      autofill_metrics::MandatoryReauthOfferOptInDecision::kAlreadyOptedIn, 1);
}

// Test that
// `PersonalDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo()`
// returns that we should not show the promo if the user has already opted out.
TEST_F(PersonalDataManagerTest,
       ShouldShowPaymentMethodsMandatoryReauthPromo_UserOptedOut) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
  base::HistogramTester histogram_tester;
  // Simulate user is already opted out.
  personal_data_->SetPaymentMethodsMandatoryReauthEnabled(false);

  EXPECT_FALSE(personal_data_->ShouldShowPaymentMethodsMandatoryReauthPromo());
  histogram_tester.ExpectUniqueSample(
      "Autofill.PaymentMethods.MandatoryReauth.CheckoutFlow."
      "ReauthOfferOptInDecision2",
      autofill_metrics::MandatoryReauthOfferOptInDecision::kAlreadyOptedOut, 1);
}

// Test that
// `PersonalDataManager::ShouldShowPaymentMethodsMandatoryReauthPromo()`
// returns that we should not show the promo if the flag is off.
TEST_F(PersonalDataManagerTest,
       ShouldShowPaymentMethodsMandatoryReauthPromo_FlagOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAutofillEnablePaymentsMandatoryReauth);
  EXPECT_FALSE(personal_data_->ShouldShowPaymentMethodsMandatoryReauthPromo());
}
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)

TEST_F(PersonalDataManagerTest, NoIbansAddedIfDisabled) {
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));

  personal_data_->AddAsLocalIban(iban);
  personal_data_->AddAsLocalIban(iban1);

  EXPECT_EQ(0U, personal_data_->GetLocalIbans().size());
}

// Ensure that new IBANs can be updated and saved via
// `OnAcceptedLocalIbanSave()`.
TEST_F(PersonalDataManagerTest, OnAcceptedLocalIbanSave) {
  // Start with a new IBAN.
  Iban iban0;
  iban0.set_value(std::u16string(test::kIbanValue16));
  // Add the IBAN to the database.
  std::string guid = personal_data_->OnAcceptedLocalIbanSave(iban0);
  iban0.set_identifier(Iban::Guid(guid));
  iban0.set_record_type(Iban::kLocalIban);

  // Make sure everything is set up correctly.
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetLocalIbans().size());

  // Creates a new IBAN and call `OnAcceptedLocalIbanSave()` and verify that
  // the new IBAN is saved.
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  guid = personal_data_->OnAcceptedLocalIbanSave(iban1);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  iban1.set_identifier(Iban::Guid(guid));
  iban1.set_record_type(Iban::kLocalIban);

  // Expect that the new IBAN is added.
  ASSERT_EQ(2U, personal_data_->GetLocalIbans().size());

  std::vector<const Iban*> ibans;
  ibans.push_back(&iban0);
  ibans.push_back(&iban1);
  // Verify that we've loaded the IBAN from the web database.
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  // Creates a new `iban2` which has the same value as `iban0` but with
  // different nickname and call `OnAcceptedLocalIbanSave()`.
  Iban iban2 = iban0;
  iban2.set_nickname(u"Nickname 2");
  personal_data_->OnAcceptedLocalIbanSave(iban2);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  // Updates the nickname for `iban1` and call `OnAcceptedLocalIbanSave()`.
  iban1.set_nickname(u"Nickname 1 updated");
  personal_data_->OnAcceptedLocalIbanSave(iban1);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  ibans.clear();
  ibans.push_back(&iban1);
  ibans.push_back(&iban2);
  // Expect that the existing IBANs are updated.
  ASSERT_EQ(2U, personal_data_->GetLocalIbans().size());

  // Verify that we've loaded the IBANs from the web database.
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  // Call `OnAcceptedLocalIbanSave()` with the same iban1, verify that nothing
  // changes.
  personal_data_->OnAcceptedLocalIbanSave(iban1);
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  // Reset the PersonalDataManager. This tests that the IBANs are persisted
  // in the local web database even if the browser is re-loaded, ensuring that
  // the user can load the IBANs from the local web database on browser startup.
  ResetPersonalDataManager();
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());
}

// Test that ensure local data is not lost on sign-in.
// Clearing/changing the primary account is not supported on CrOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PersonalDataManagerTest, KeepExistingLocalDataOnSignIn) {
  // Sign out.
  identity_test_env_.ClearPrimaryAccount();
  sync_service_.SetAccountInfo(CoreAccountInfo());
  EXPECT_TRUE(sync_service_.GetAccountInfo().IsEmpty());
  EXPECT_EQ(0U, personal_data_->GetCreditCards().size());

  // Add local card.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                          "4234567890123463",  // Visa
                          "08", "2999", "1");
  local_card.set_guid("00000000-0000-0000-0000-000000000009");
  local_card.set_record_type(CreditCard::RecordType::kLocalCard);
  local_card.set_use_count(5);
  personal_data_->AddCreditCard(local_card);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  // Sign in.
  identity_test_env_.MakePrimaryAccountAvailable("test@gmail.com",
                                                 signin::ConsentLevel::kSync);
  sync_service_.SetAccountInfo(
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSync));
  sync_service_.SetHasSyncConsent(true);
  EXPECT_TRUE(
      sync_service_.IsSyncFeatureEnabled() &&
      sync_service_.GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA));
  ASSERT_TRUE(TurnOnSyncFeature());

  // Check saved local card should be not lost.
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(0, local_card.Compare(*personal_data_->GetCreditCards()[0]));
}
#endif

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

// Ensure that verified credit cards can be saved via
// OnAcceptedLocalCreditCardSave.
TEST_F(PersonalDataManagerTest, OnAcceptedLocalCreditCardSaveWithVerifiedData) {
  // Start with a verified credit card.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  EXPECT_TRUE(credit_card.IsVerified());

  // Add the credit card to the database.
  personal_data_->AddCreditCard(credit_card);

  // Make sure everything is set up correctly.
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard new_verified_card = credit_card;
  new_verified_card.set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  new_verified_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"B. Small");
  EXPECT_TRUE(new_verified_card.IsVerified());

  personal_data_->OnAcceptedLocalCreditCardSave(new_verified_card);

  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Expect that the saved credit card is updated.
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(u"B. Small", results[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
}

// Tests that GetAutofillOffers does not return any offers if
// |IsAutofillWalletImportEnabled()| returns |false|.
TEST_F(PersonalDataManagerTest, GetAutofillOffers_WalletImportDisabled) {
  // Add a card-linked offer and a promo code offer.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  AddOfferDataForTest(test::GetPromoCodeOfferData());

  ASSERT_EQ(2U, personal_data_->GetAutofillOffers().size());

  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  // Should return neither of them as the wallet import pref is disabled.
  EXPECT_EQ(0U, personal_data_->GetAutofillOffers().size());
}

// Tests that GetAutofillOffers does not return any offers if
// `IsAutofillPaymentMethodsEnabled()` returns `false`.
TEST_F(PersonalDataManagerTest, GetAutofillOffers_AutofillCreditCardDisabled) {
  // Add a card-linked offer and a promo code offer.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  AddOfferDataForTest(test::GetPromoCodeOfferData());

  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Should return neither of the offers as the autofill credit card import pref
  // is disabled.
  EXPECT_EQ(0U, personal_data_->GetAutofillOffers().size());
}

// Tests that GetActiveAutofillPromoCodeOffersForOrigin does not return any
// promo code offers if |IsAutofillWalletImportEnabled()| returns |false|.
TEST_F(PersonalDataManagerTest,
       GetActiveAutofillPromoCodeOffersForOrigin_WalletImportDisabled) {
  // Add an active promo code offer.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com")));

  ASSERT_EQ(1U, personal_data_
                    ->GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());

  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, syncer::UserSelectableTypeSet());

  // Should not return the offer as the wallet import pref is disabled.
  EXPECT_EQ(0U, personal_data_
                    ->GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());
}

// Tests that GetActiveAutofillPromoCodeOffersForOrigin does not return any
// promo code offers if `IsAutofillPaymentMethodsEnabled()` returns `false`.
TEST_F(PersonalDataManagerTest,
       GetActiveAutofillPromoCodeOffersForOrigin_AutofillCreditCardDisabled) {
  // Add an active promo code offer.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com")));

  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Should not return the offer as the autofill credit card pref is disabled.
  EXPECT_EQ(0U, personal_data_
                    ->GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());
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

TEST_F(PersonalDataManagerTest, IsKnownCard_MatchesMaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "2110" /* last 4 digits */, "12", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_MatchesFullServerCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "4234567890122110" /* Visa */, "12", "2999", "1");

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_MatchesLocalCard) {
  // Add a local card.
  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "4234 5678 9012 2110" /* Visa */, "04", "2999", "1");
  personal_data_->AddCreditCard(credit_card0);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234567890122110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_TypeDoesNotMatch) {
  // Add a local card.
  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "4234 5678 9012 2110" /* Visa */, "04", "2999", "1");
  personal_data_->AddCreditCard(credit_card0);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"5105 1051 0510 2110" /* American Express */);
  ASSERT_FALSE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsKnownCard_LastFourDoesNotMatch) {
  // Add a local card.
  CreditCard credit_card0("287151C8-6AB1-487C-9095-28E80BE5DA15",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "Clyde Barrow",
                          "4234 5678 9012 2110" /* Visa */, "04", "2999", "1");
  personal_data_->AddCreditCard(credit_card0);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 0000" /* Visa */);
  ASSERT_FALSE(personal_data_->IsKnownCard(cardToCompare));
}

TEST_F(PersonalDataManagerTest, IsServerCard_DuplicateOfFullServerCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "4234567890122110" /* Visa */, "12", "2999", "1");

  SetServerCards(server_cards);

  // Add a dupe local card of a full server card.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Emmet Dalton",
                          "4234 5678 9012 2110" /* Visa */, "12", "2999", "1");
  personal_data_->AddCreditCard(local_card);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsServerCard(&cardToCompare));
  ASSERT_TRUE(personal_data_->IsServerCard(&local_card));
}

TEST_F(PersonalDataManagerTest, IsServerCard_DuplicateOfMaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton",
                          "2110" /* last 4 digits */, "12", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCards(server_cards);

  // Add a dupe local card of a full server card.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Emmet Dalton",
                          "4234 5678 9012 2110" /* Visa */, "12", "2999", "1");
  personal_data_->AddCreditCard(local_card);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  CreditCard cardToCompare;
  cardToCompare.SetNumber(u"4234 5678 9012 2110" /* Visa */);
  ASSERT_TRUE(personal_data_->IsServerCard(&cardToCompare));
  ASSERT_TRUE(personal_data_->IsServerCard(&local_card));
}

TEST_F(PersonalDataManagerTest, IsServerCard_AlreadyServerCard) {
  std::vector<CreditCard> server_cards;
  // Create a full server card.
  CreditCard full_server_card(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  server_cards.push_back(full_server_card);
  // Create a masked server card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "2110" /* Visa */,
                          "01", "2999", "1");
  masked_card.SetNetworkForMaskedCard(kVisaCard);
  server_cards.push_back(masked_card);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());

  ASSERT_TRUE(personal_data_->IsServerCard(&full_server_card));
  ASSERT_TRUE(personal_data_->IsServerCard(&masked_card));
}

TEST_F(PersonalDataManagerTest, IsServerCard_UniqueLocalCard) {
  // Add a unique local card.
  CreditCard local_card("1141084B-72D7-4B73-90CF-3D6AC154673B",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  personal_data_->AddCreditCard(local_card);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  ASSERT_FALSE(personal_data_->IsServerCard(&local_card));
}

// Test that local and server cards are not shown if
// |kAutofillCreditCardEnabled| is set to |false|.
TEST_F(PersonalDataManagerTest,
       GetCreditCardsToSuggest_CreditCardAutofillDisabled) {
  SetUpReferenceLocalCreditCards();

  // Add some server cards.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "b460");
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));

  SetServerCards(server_cards);
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Disable Credit card autofill.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Check that profiles were saved.
  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());
  // Expect no autofilled values or suggestions.
  EXPECT_EQ(0U, personal_data_->GetCreditCardsToSuggest().size());

  std::vector<CreditCard*> card_to_suggest =
      personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(0U, card_to_suggest.size());
}

// Test that local and server cards are not loaded into memory on start-up if
// |kAutofillCreditCardEnabled| is set to |false|.
TEST_F(PersonalDataManagerTest,
       GetCreditCardsToSuggest_NoCardsLoadedIfDisabled) {
  SetUpReferenceLocalCreditCards();

  // Add some server cards.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b459");
  test::SetCreditCardInfo(&server_cards.back(), "Emmet Dalton", "2110", "12",
                          "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "b460");
  test::SetCreditCardInfo(&server_cards.back(), "Jesse James", "2109", "12",
                          "2999", "1");
  server_cards.back().set_use_count(6);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(1));

  SetServerCards(server_cards);

  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Expect 5 autofilled values or suggestions.
  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());

  // Disable Credit card autofill.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  // Reload the database.
  ResetPersonalDataManager();

  // Expect no credit card values or suggestions were loaded.
  EXPECT_EQ(0U, personal_data_->GetCreditCardsToSuggest().size());

  std::vector<CreditCard*> card_to_suggest =
      personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(0U, card_to_suggest.size());
}

// Test that local credit cards are not added if |kAutofillCreditCardEnabled| is
// set to |false|.
TEST_F(PersonalDataManagerTest,
       GetCreditCardsToSuggest_NoCreditCardsAddedIfDisabled) {
  // Disable Profile autofill.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Add a local credit card.
  CreditCard credit_card("002149C1-EE28-4213-A3B9-DA243FFF021B",
                         "https://www.example.com");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "04", "2999",
                          "1");
  personal_data_->AddCreditCard(credit_card);

  // Expect no credit card values or suggestions were added.
  EXPECT_EQ(0U, personal_data_->GetCreditCards().size());
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

// Test that setting a null sync service returns only local credit cards.
TEST_F(PersonalDataManagerTest, GetCreditCards_NoSyncService) {
  base::HistogramTester histogram_tester;
  SetUpTwoCardTypes();

  // Set no sync service.
  personal_data_->SetSyncServiceForTest(nullptr);
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // No sync service is the same as payments integration being disabled, i.e.
  // IsAutofillWalletImportEnabled() returning false. Only local credit
  // cards are shown.
  EXPECT_EQ(0U, personal_data_->GetServerCreditCards().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
}

// Sync Transport mode is only for Win, Mac, and Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
TEST_F(PersonalDataManagerSyncTransportModeTest,
       ServerCardsShowInTransportMode) {
  SetUpTwoCardTypes();

  CoreAccountInfo active_info =
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  // Opt-in to seeing server card in sync transport mode.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), active_info.account_id, true);

  // Check that the server card is available for suggestion.
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(1U, personal_data_->GetServerCreditCards().size());

  // Stop Wallet sync.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());

  // Check that server cards are unavailable.
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(0U, personal_data_->GetServerCreditCards().size());
}

// Make sure that the opt in is necessary to show server cards if the
// appropriate feature is disabled.
TEST_F(PersonalDataManagerSyncTransportModeTest,
       ServerCardsShowInTransportMode_NeedOptIn) {
  SetUpTwoCardTypes();

  CoreAccountInfo active_info =
      identity_test_env_.identity_manager()->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSignin);

  // The server card should not be available at first. The user needs to
  // accept the opt-in offer.
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(1U, personal_data_->GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(1U, personal_data_->GetServerCreditCards().size());

  // Opt-in to seeing server card in sync transport mode.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(
      prefs_.get(), active_info.account_id, true);

  // Check that the server card is available for suggestion.
  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(1U, personal_data_->GetServerCreditCards().size());
}

TEST_F(PersonalDataManagerSyncTransportModeTest,
       AutofillSyncToggleAvailableInTransportMode) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{syncer::
                                kSyncEnableContactInfoDataTypeInTransportMode,
                            syncer::kSyncDecoupleAddressPaymentSettings,
                            ::switches::kExplicitBrowserSigninUIOnDesktop},
      /*disabled_features=*/{});

  prefs_->SetBoolean(::prefs::kExplicitBrowserSignin, true);
  EXPECT_TRUE(personal_data_->IsAutofillSyncToggleAvailable());

  prefs_->SetBoolean(::prefs::kExplicitBrowserSignin, false);
  EXPECT_FALSE(personal_data_->IsAutofillSyncToggleAvailable());
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// Tests that all the non settings origins of autofill credit cards are cleared
// even if sync is disabled.
TEST_F(
    PersonalDataManagerTest,
    SyncServiceInitializedWithAutofillDisabled_ClearCreditCardNonSettingsOrigins) {
  // Create a card with a non-settings, non-empty origin.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         "https://www.example.com");
  test::SetCreditCardInfo(&credit_card, "Bob0",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  personal_data_->AddCreditCard(credit_card);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Turn off payments sync.
  syncer::UserSelectableTypeSet user_selectable_type_set =
      sync_service_.GetUserSettings()->GetSelectedTypes();
  user_selectable_type_set.Remove(syncer::UserSelectableType::kPayments);
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/user_selectable_type_set);

  // The credit card should still exist.
  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

  // Reload the personal data manager.
  ResetPersonalDataManager();

  // The credit card should still exist.
  ASSERT_EQ(1U, personal_data_->GetCreditCards().size());

  // The card's origin should be cleared
  EXPECT_TRUE(personal_data_->GetCreditCards()[0]->origin().empty());
}

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

  personal_data_->OnURLsDeleted(/*history_service=*/nullptr, deletion_info);

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

  personal_data_->OnURLsDeleted(/*history_service=*/nullptr, deletion_info);

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

  personal_data_->OnURLsDeleted(/*history_service=*/nullptr, deletion_info);

  // The strikes for `first_url` should be deleted because the strikes have been
  // added within the deletion time range.
  EXPECT_FALSE(adm.IsNewProfileImportBlockedForDomain(first_url));
  // The last strike for 'second_url' was collected after the deletion time
  // range and therefore, the blocking should prevail.
  EXPECT_TRUE(adm.IsNewProfileImportBlockedForDomain(second_url));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PersonalDataManagerSyncTransportModeTest,
       ShouldShowCardsFromAccountOption) {
  // The method should return false if one of these is not respected:
  //   * The sync_service is not null
  //   * The sync feature is not enabled
  //   * The user has server cards
  //   * The user has not opted-in to seeing their account cards
  // Start by setting everything up, then making each of these conditions false
  // independently, one by one.

  // Set everything up so that the proposition should be shown.

  // Set a server credit card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  SetServerCards(server_cards);
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Make sure the function returns true.
  EXPECT_TRUE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user already opted-in. Check that the function now returns
  // false.
  CoreAccountId account_id =
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin);
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(prefs_.get(), account_id,
                                                       true);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-opt the user out. Check that the function now returns true.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(prefs_.get(), account_id,
                                                       false);
  EXPECT_TRUE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user has no server cards. Check that the function now returns
  // false.
  SetServerCards({});
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-set some server cards. Check that the function now returns true.
  SetServerCards(server_cards);
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_TRUE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user enabled the sync feature. Check that the function now
  // returns false.
  sync_service_.SetHasSyncConsent(true);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-disable the sync feature. Check that the function now returns true.
  sync_service_.SetHasSyncConsent(false);
  EXPECT_TRUE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set a null sync service. Check that the function now returns false.
  personal_data_->SetSyncServiceForTest(nullptr);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());
}
#else   // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) &&
        // !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PersonalDataManagerSyncTransportModeTest,
       ShouldShowCardsFromAccountOption) {
  // The method should return false if one of these is not respected:
  //   * The sync_service is not null
  //   * The sync feature is not enabled
  //   * The user has server cards
  //   * The user has not opted-in to seeing their account cards
  // Start by setting everything up, then making each of these conditions false
  // independently, one by one.

  // Set everything up so that the proposition should be shown on Desktop.

  // Set a server credit card.
  std::vector<CreditCard> server_cards;
  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  SetServerCards(server_cards);
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Make sure the function returns false.
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user already opted-in. Check that the function still returns
  // false.
  CoreAccountId account_id =
      identity_test_env_.identity_manager()->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin);
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(prefs_.get(), account_id,
                                                       true);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-opt the user out. Check that the function now returns true.
  ::autofill::prefs::SetUserOptedInWalletSyncTransport(prefs_.get(), account_id,
                                                       false);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user has no server cards. Check that the function still
  // returns false.
  SetServerCards({});
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-set some server cards. Check that the function still returns false.
  SetServerCards(server_cards);
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set that the user enabled the sync feature. Check that the function still
  // returns false.
  sync_service_.SetHasSyncConsent(true);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-disable the sync feature. Check that the function still returns false.
  sync_service_.SetHasSyncConsent(false);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Set a null sync service. Check that the function still returns false.
  personal_data_->SetSyncServiceForTest(nullptr);
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS) &&
        // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PersonalDataManagerSyncTransportModeTest,
       GetPaymentsSigninStateForMetrics) {
  // Make sure a non-sync-consented account is available for the first tests.
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSignin));
  ASSERT_FALSE(sync_service_.HasSyncConsent());
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/{syncer::UserSelectableType::kAutofill,
                 syncer::UserSelectableType::kPayments});

  EXPECT_EQ(AutofillMetrics::PaymentsSigninState::
                kSignedInAndWalletSyncTransportEnabled,
            personal_data_->GetPaymentsSigninStateForMetrics());

  // Check that the sync state is |SignedIn| if the sync service does not have
  // wallet data active.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet(
          {syncer::UserSelectableType::kAutofill}));
  EXPECT_EQ(AutofillMetrics::PaymentsSigninState::kSignedIn,
            personal_data_->GetPaymentsSigninStateForMetrics());

  // Nothing should change if |kAutofill| is also removed.
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_EQ(AutofillMetrics::PaymentsSigninState::kSignedIn,
            personal_data_->GetPaymentsSigninStateForMetrics());

// ClearPrimaryAccount is not supported on CrOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Check that the sync state is |SignedOut| when the account info is empty.
  {
    identity_test_env_.ClearPrimaryAccount();
    sync_service_.SetAccountInfo(CoreAccountInfo());
    sync_service_.SetHasSyncConsent(false);
    EXPECT_EQ(AutofillMetrics::PaymentsSigninState::kSignedOut,
              personal_data_->GetPaymentsSigninStateForMetrics());
  }
#endif

  // Simulate that the user has enabled the sync feature.
  AccountInfo primary_account_info;
  primary_account_info.email = kPrimaryAccountEmail;
  sync_service_.SetAccountInfo(primary_account_info);
  sync_service_.SetHasSyncConsent(true);
// MakePrimaryAccountAvailable is not supported on CrOS.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  identity_test_env_.MakePrimaryAccountAvailable(primary_account_info.email,
                                                 signin::ConsentLevel::kSync);
#endif

  // Check that the sync state is |SignedInAndSyncFeature| if the sync feature
  // is enabled.
  EXPECT_EQ(
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled,
      personal_data_->GetPaymentsSigninStateForMetrics());
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

}  // namespace autofill
