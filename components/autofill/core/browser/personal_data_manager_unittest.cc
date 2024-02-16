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

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
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
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
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
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace autofill {

namespace {

using testing::ElementsAre;
using testing::Pointee;
using testing::UnorderedElementsAre;

constexpr char kGuid[] = "a21f010a-eac1-41fc-aee9-c06bbedfb292";
constexpr char kPrimaryAccountEmail[] = "syncuser@example.com";

const base::Time kArbitraryTime = base::Time::FromSecondsSinceUnixEpoch(25);
const base::Time kSomeLaterTime = base::Time::FromSecondsSinceUnixEpoch(1000);

class PersonalDataManagerMock : public PersonalDataManager {
 public:
  explicit PersonalDataManagerMock(const std::string& app_locale,
                                   const std::string& variations_country_code)
      : PersonalDataManager(app_locale, variations_country_code) {}
  ~PersonalDataManagerMock() override = default;

  MOCK_METHOD(void,
              FetchImagesForURLs,
              ((base::span<const GURL> updated_urls)),
              (const override));
};

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
    PersonalDataProfileTaskWaiter waiter(*personal_data_);
    EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
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
      PersonalDataProfileTaskWaiter waiter(*personal_data_);
      EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
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
      PersonalDataProfileTaskWaiter waiter(*personal_data_);
      EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
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
    PersonalDataProfileTaskWaiter waiter(*personal_data_);
    EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
    personal_data_->AddProfile(profile);
    std::move(waiter).Wait();
  }

  void RemoveByGUIDFromPersonalDataManager(const std::string& guid) {
    PersonalDataProfileTaskWaiter waiter(*personal_data_);
    EXPECT_CALL(waiter.mock_observer(), OnPersonalDataChanged());
    personal_data_->RemoveByGUID(guid);
    std::move(waiter).Wait();
  }

  void SetServerCards(const std::vector<CreditCard>& server_cards) {
    test::SetServerCreditCards(GetServerDataTable(), server_cards);
  }

  void AddOfferDataForTest(AutofillOfferData offer_data) {
    personal_data_->AddOfferDataForTest(
        std::make_unique<AutofillOfferData>(offer_data));
  }

  void AddLocalIban(Iban& iban) {
    iban.set_identifier(Iban::Guid(personal_data_->AddAsLocalIban(iban)));
    PersonalDataProfileTaskWaiter(*personal_data_).Wait();
    iban.set_record_type(Iban::kLocalIban);
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

class PersonalDataManagerMockTest : public PersonalDataManagerTestBase,
                                    public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager();
  }

  void TearDown() override {
    if (personal_data_) {
      personal_data_->Shutdown();
    }
    personal_data_.reset();
    TearDownTest();
  }

  void ResetPersonalDataManager() {
    if (personal_data_) {
      personal_data_->Shutdown();
    }
    personal_data_ =
        std::make_unique<PersonalDataManagerMock>("en", std::string());
    PersonalDataManagerTestBase::ResetPersonalDataManager(
        /*use_sync_transport_mode=*/true, personal_data_.get());
  }

  // Verifies the credit card art image fetching should begin.
  void WaitForFetchImagesForUrls() {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    EXPECT_CALL(*personal_data_, FetchImagesForURLs(testing::_))
        .Times(1)
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
    run_loop.Run();
  }

  std::unique_ptr<PersonalDataManagerMock> personal_data_;
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

  EXPECT_THAT(
      personal_data_->GetProfilesForSettings(),
      ElementsAre(Pointee(kLocalOrSyncableProfile), Pointee(kAccountProfile)));
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

// Test that server IBANs can be added and automatically loaded/cached.
TEST_F(PersonalDataManagerTest, AddAndReloadServerIbans) {
  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();

  GetServerDataTable()->SetServerIbansForTesting({server_iban1, server_iban2});
  std::vector<const Iban*> expected_ibans = {&server_iban1, &server_iban2};
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  ExpectSameElements(expected_ibans, personal_data_->GetServerIbans());

  // Reset the PersonalDataManager. This tests that the personal data was saved
  // to the web database, and that we can load the IBANs from the web database.
  ResetPersonalDataManager();

  // Verify that we've reloaded the IBANs from the web database.
  ExpectSameElements(expected_ibans, personal_data_->GetServerIbans());
}

// Test that all (local and server) IBANs can be returned.
TEST_F(PersonalDataManagerTest, GetIbans) {
  personal_data_->SetSyncingForTest(true);

  Iban local_iban1;
  local_iban1.set_value(std::u16string(test::kIbanValue16));
  Iban local_iban2;
  local_iban2.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();

  AddLocalIban(local_iban1);
  AddLocalIban(local_iban2);

  GetServerDataTable()->SetServerIbansForTesting({server_iban1, server_iban2});
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  std::vector<const Iban*> all_ibans = {&local_iban1, &local_iban2,
                                        &server_iban1, &server_iban2};
  ExpectSameElements(all_ibans, personal_data_->GetIbans());
}

// Test that deduplication works correctly when a local IBAN has a matching
// prefix and suffix (either equal or starting with) and the same length as the
// server IBANs.
TEST_F(PersonalDataManagerTest, GetIbansToSuggest) {
  personal_data_->SetSyncingForTest(true);

  // Create two IBANs, and two server IBANs.
  // `local_iban1` and `server_iban1` have the same prefix, suffix and length.
  Iban local_iban1;
  local_iban1.set_value(u"FR76 3000 6000 0112 3456 7890 189");
  Iban local_iban2;
  local_iban2.set_value(u"CH56 0483 5012 3456 7800 9");
  Iban server_iban1(Iban::InstrumentId(1234567));
  server_iban1.set_prefix(u"FR76");
  server_iban1.set_suffix(u"0189");
  server_iban1.set_length(27);
  Iban server_iban2 = test::GetServerIban2();
  server_iban2.set_length(34);

  AddLocalIban(local_iban1);
  AddLocalIban(local_iban2);

  GetServerDataTable()->SetServerIbansForTesting({server_iban1, server_iban2});
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  std::vector<const Iban*> ibans_to_suggest = {&server_iban1, &server_iban2,
                                               &local_iban2};
  ExpectSameElements(ibans_to_suggest, personal_data_->GetIbansToSuggest());
}

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

TEST_F(PersonalDataManagerTest, AddingIbanUpdatesPref) {
  // The pref should always start disabled.
  ASSERT_FALSE(personal_data_->IsAutofillHasSeenIbanPrefEnabled());
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));

  personal_data_->AddAsLocalIban(iban);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  // Adding an IBAN permanently enables the pref.
  EXPECT_TRUE(personal_data_->IsAutofillHasSeenIbanPrefEnabled());
}

TEST_F(PersonalDataManagerTest, AddLocalIbans) {
  Iban iban1;
  iban1.set_value(std::u16string(test::kIbanValue16));
  iban1.set_nickname(u"Nickname for Iban");

  Iban iban2;
  iban2.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  iban2.set_nickname(u"Original nickname");

  Iban iban2_with_different_nickname = iban2;
  iban2_with_different_nickname.set_nickname(u"Different nickname");

  // Attempt to add all three IBANs to the database. The first two should add
  // successfully, but the third should get skipped because its value is
  // identical to `iban2`.
  AddLocalIban(iban1);
  AddLocalIban(iban2);
  // Do not add `PersonalDataProfileTaskWaiter(*personal_data_).Wait()` for this
  // `AddAsLocalIban` operation, as it will be terminated prematurely for
  // `iban2_with_different_nickname` due to the presence of an IBAN with the
  // same value.
  personal_data_->AddAsLocalIban(iban2_with_different_nickname);

  std::vector<const Iban*> ibans = {&iban1, &iban2};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());
}

TEST_F(PersonalDataManagerTest, UpdateLocalIbans) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  iban.set_nickname(u"Nickname for Iban");
  AddLocalIban(iban);

  // Verify the `iban` has been added successfully.
  std::vector<const Iban*> ibans = {&iban};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  // Update the `iban` with new value.
  iban.SetRawInfo(IBAN_VALUE, u"GB98 MIDL 0700 9312 3456 78");
  personal_data_->UpdateIban(iban);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ibans = {&iban};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  // Update the `iban` with new nickname.
  iban.set_nickname(u"Another nickname");
  personal_data_->UpdateIban(iban);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ibans = {&iban};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());
}

TEST_F(PersonalDataManagerTest, RemoveLocalIbans) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  iban.set_nickname(u"Nickname for Iban");
  AddLocalIban(iban);

  // Verify the `iban` has been added successfully.
  std::vector<const Iban*> ibans = {&iban};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  RemoveByGUIDFromPersonalDataManager(iban.guid());
  EXPECT_TRUE(personal_data_->GetLocalIbans().empty());

  // Verify that removal of a GUID that doesn't exist won't crash.
  RemoveByGUIDFromPersonalDataManager(iban.guid());
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetLocalIbans().size());

  // Creates a new IBAN and call `OnAcceptedLocalIbanSave()` and verify that
  // the new IBAN is saved.
  Iban iban1;
  iban1.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue_1)));
  guid = personal_data_->OnAcceptedLocalIbanSave(iban1);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  // Updates the nickname for `iban1` and call `OnAcceptedLocalIbanSave()`.
  iban1.set_nickname(u"Nickname 1 updated");
  personal_data_->OnAcceptedLocalIbanSave(iban1);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

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

TEST_F(PersonalDataManagerTest, RecordIbanUsage_LocalIban) {
  base::HistogramTester histogram_tester;
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  Iban local_iban;
  local_iban.set_value(u"FR76 3000 6000 0112 3456 7890 189");
  EXPECT_EQ(local_iban.use_count(), 1u);
  EXPECT_EQ(local_iban.use_date(), kArbitraryTime);
  EXPECT_EQ(local_iban.modification_date(), kArbitraryTime);

  AddLocalIban(local_iban);

  // Set the current time to sometime later.
  test_clock.SetNow(kSomeLaterTime);

  // Use `local_iban`, then verify usage stats.
  EXPECT_EQ(personal_data_->GetLocalIbans().size(), 1u);
  personal_data_->RecordUseOfIban(local_iban);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredIban.Local", 1);
  EXPECT_EQ(local_iban.use_count(), 2u);
  EXPECT_EQ(local_iban.use_date(), kSomeLaterTime);
  EXPECT_EQ(local_iban.modification_date(), kArbitraryTime);
}

TEST_F(PersonalDataManagerTest, RecordIbanUsage_ServerIban) {
  base::HistogramTester histogram_tester;
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  Iban server_iban = test::GetServerIban();
  EXPECT_EQ(server_iban.use_count(), 1u);
  EXPECT_EQ(server_iban.use_date(), kArbitraryTime);
  EXPECT_EQ(server_iban.modification_date(), kArbitraryTime);
  GetServerDataTable()->SetServerIbansForTesting({server_iban});
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Set the current time to sometime later.
  test_clock.SetNow(kSomeLaterTime);

  // Use `server_iban`, then verify usage stats.
  EXPECT_EQ(personal_data_->GetServerIbans().size(), 1u);
  personal_data_->RecordUseOfIban(server_iban);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredIban.Server", 1);
  EXPECT_EQ(server_iban.use_count(), 2u);
  EXPECT_EQ(server_iban.use_date(), kSomeLaterTime);
  EXPECT_EQ(server_iban.modification_date(), kArbitraryTime);
}

TEST_F(PersonalDataManagerTest, AddUpdateRemoveCreditCards) {
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  credit_card0.SetNickname(u"card zero");

  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Bonnie Parker",
                          "5105105105105100" /* Mastercard */, "12", "2999",
                          "1");

  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card2.SetNickname(u"card two");

  // Add two test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Update, remove, and add.
  credit_card0.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Joe");
  credit_card0.SetNickname(u"new card zero");
  personal_data_->UpdateCreditCard(credit_card0);
  RemoveByGUIDFromPersonalDataManager(credit_card1.guid());
  personal_data_->AddCreditCard(credit_card2);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  cards.clear();
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card2);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  // Verify that we've loaded the credit cards from the web database.
  cards.clear();
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card2);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Add a full server card.
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Jane Doe",
                          "4111111111111111" /* Visa */, "04", "2999", "1");
  credit_card3.set_record_type(CreditCard::RecordType::kFullServerCard);
  credit_card3.set_server_id("server_id");

  personal_data_->AddFullServerCreditCardForTesting(credit_card3);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  cards.push_back(&credit_card3);
  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Must not add a duplicate server card with same GUID.
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);

  personal_data_->AddFullServerCreditCardForTesting(credit_card3);

  ExpectSameElements(cards, personal_data_->GetCreditCards());

  // Must not add a duplicate card with same contents as another server card.
  CreditCard duplicate_server_card(credit_card3);
  duplicate_server_card.set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());

  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(0);

  personal_data_->AddFullServerCreditCardForTesting(duplicate_server_card);

  ExpectSameElements(cards, personal_data_->GetCreditCards());
}

// Test that UpdateLocalCvc function working as expected.
TEST_F(PersonalDataManagerTest, UpdateLocalCvc) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  CreditCard credit_card = test::GetCreditCard();
  const std::u16string kCvc = u"111";
  credit_card.set_cvc(kCvc);
  PersonalDataProfileTaskWaiter add_waiter(*personal_data_);
  personal_data_->AddCreditCard(credit_card);
  std::move(add_waiter).Wait();
  ASSERT_EQ(personal_data_->GetLocalCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetLocalCreditCards()[0]->cvc(), kCvc);

  const std::u16string kNewCvc = u"222";
  PersonalDataProfileTaskWaiter update_waiter(*personal_data_);
  personal_data_->UpdateLocalCvc(credit_card.guid(), kNewCvc);
  std::move(update_waiter).Wait();
  ASSERT_EQ(personal_data_->GetLocalCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetLocalCreditCards()[0]->cvc(), kNewCvc);
}

// Test that verify add, update, remove server cvc function working as expected.
TEST_F(PersonalDataManagerTest, ServerCvc) {
  const std::u16string kCvc = u"111";
  CreditCard credit_card = test::GetMaskedServerCard();
  SetServerCards({credit_card});

  // Add an empty cvc will fail a CHECK().
  EXPECT_DEATH_IF_SUPPORTED(personal_data_->AddServerCvc(1, u""), "");

  personal_data_->AddServerCvc(credit_card.instrument_id(), kCvc);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  ASSERT_EQ(personal_data_->GetCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetCreditCards()[0]->cvc(), kCvc);

  // Update an empty cvc will fail a CHECK().
  EXPECT_DEATH_IF_SUPPORTED(
      personal_data_->UpdateServerCvc(credit_card.instrument_id(), u""), "");
  // Update an non-exist card cvc will fail a CHECK().
  EXPECT_DEATH_IF_SUPPORTED(personal_data_->UpdateServerCvc(99999, u""), "");

  const std::u16string kNewCvc = u"222";
  personal_data_->UpdateServerCvc(credit_card.instrument_id(), kNewCvc);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(personal_data_->GetCreditCards()[0]->cvc(), kNewCvc);

  personal_data_->RemoveServerCvc(credit_card.instrument_id());
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  ASSERT_EQ(personal_data_->GetCreditCards().size(), 1U);
  EXPECT_TRUE(personal_data_->GetCreditCards()[0]->cvc().empty());
}

// Test that verify clear server cvc function working as expected.
TEST_F(PersonalDataManagerTest, ClearServerCvc) {
  // Add a server card cvc.
  const std::u16string kCvc = u"111";
  CreditCard credit_card = test::GetMaskedServerCard();
  SetServerCards({credit_card});
  personal_data_->AddServerCvc(credit_card.instrument_id(), kCvc);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  ASSERT_EQ(personal_data_->GetCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetCreditCards()[0]->cvc(), kCvc);

  // After we clear server cvcs we should expect empty cvc.
  personal_data_->ClearServerCvcs();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_TRUE(personal_data_->GetCreditCards()[0]->cvc().empty());
}

// Test that a new credit card has its basic information set.
TEST_F(PersonalDataManagerTest, AddCreditCard_BasicInformation) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  // Add a credit card to the database.
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card, "John Dillinger",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  personal_data_->AddCreditCard(credit_card);

  // Reload the database.
  ResetPersonalDataManager();

  // Verify the addition.
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, credit_card.Compare(*results[0]));

  // Make sure the use count and use date were set.
  EXPECT_EQ(1U, results[0]->use_count());
  EXPECT_EQ(kArbitraryTime, results[0]->use_date());
  EXPECT_EQ(kArbitraryTime, results[0]->modification_date());
}

// Test filling credit cards with unicode strings and crazy characters.
TEST_F(PersonalDataManagerTest, AddCreditCard_CrazyCharacters) {
  std::vector<CreditCard> cards;
  CreditCard card1;
  card1.SetRawInfo(CREDIT_CARD_NAME_FULL,
                   u"\u751f\u6d3b\u5f88\u6709\u89c4\u5f8b "
                   u"\u4ee5\u73a9\u4e3a\u4e3b");
  card1.SetRawInfo(CREDIT_CARD_NUMBER, u"6011111111111117");
  card1.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
  card1.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2011");
  cards.push_back(card1);

  CreditCard card2;
  card2.SetRawInfo(CREDIT_CARD_NAME_FULL, u"John Williams");
  card2.SetRawInfo(CREDIT_CARD_NUMBER, u"WokoAwesome12345");
  card2.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"10");
  card2.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2015");
  cards.push_back(card2);

  CreditCard card3;
  card3.SetRawInfo(CREDIT_CARD_NAME_FULL,
                   u"\u0623\u062d\u0645\u062f\u064a "
                   u"\u0646\u062c\u0627\u062f "
                   u"\u0644\u0645\u062d\u0627\u0648\u0644\u0647 "
                   u"\u0627\u063a\u062a\u064a\u0627\u0644 "
                   u"\u0641\u064a \u0645\u062f\u064a\u0646\u0629 "
                   u"\u0647\u0645\u062f\u0627\u0646 ");
  card3.SetRawInfo(CREDIT_CARD_NUMBER,
                   u"\u092a\u0941\u0928\u0930\u094d\u091c\u0940"
                   u"\u0935\u093f\u0924 \u0939\u094b\u0917\u093e "
                   u"\u0928\u093e\u0932\u0902\u0926\u093e");
  card3.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"10");
  card3.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2015");
  cards.push_back(card3);

  CreditCard card4;
  card4.SetRawInfo(CREDIT_CARD_NAME_FULL,
                   u"\u039d\u03ad\u03b5\u03c2 "
                   u"\u03c3\u03c5\u03b3\u03c7\u03c9\u03bd\u03b5"
                   u"\u03cd\u03c3\u03b5\u03b9\u03c2 "
                   u"\u03ba\u03b1\u03b9 "
                   u"\u03ba\u03b1\u03c4\u03b1\u03c1\u03b3\u03ae"
                   u"\u03c3\u03b5\u03b9\u03c2");
  card4.SetRawInfo(CREDIT_CARD_NUMBER, u"00000000000000000000000");
  card4.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"01");
  card4.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2016");
  cards.push_back(card4);

  personal_data_->SetCreditCards(&cards);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ASSERT_EQ(cards.size(), personal_data_->GetCreditCards().size());
  for (size_t i = 0; i < cards.size(); ++i) {
    EXPECT_TRUE(base::Contains(cards, *personal_data_->GetCreditCards()[i]));
  }
}

// Test invalid credit card numbers typed in settings UI should be saved as-is.
TEST_F(PersonalDataManagerTest, AddCreditCard_Invalid) {
  CreditCard card;
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"Not_0123-5Checked");

  std::vector<CreditCard> cards;
  cards.push_back(card);
  personal_data_->SetCreditCards(&cards);

  ASSERT_EQ(1u, personal_data_->GetCreditCards().size());
  ASSERT_EQ(card, *personal_data_->GetCreditCards()[0]);
}

TEST_F(PersonalDataManagerTest, GetCreditCardByServerId) {
  CreditCard card = test::GetFullServerCard();
  card.set_server_id("server id");
  personal_data_->AddFullServerCreditCardForTesting(card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ASSERT_EQ(1u, personal_data_->GetCreditCards().size());
  EXPECT_TRUE(personal_data_->GetCreditCardByServerId("server id"));
  EXPECT_FALSE(personal_data_->GetCreditCardByServerId("non-existing id"));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(PersonalDataManagerTest, AddAndGetCreditCardArtImage) {
  gfx::Image expected_image = gfx::test::CreateImage(40, 24);
  std::unique_ptr<CreditCardArtImage> credit_card_art_image =
      std::make_unique<CreditCardArtImage>(GURL("https://www.example.com"),
                                           expected_image);
  std::vector<std::unique_ptr<CreditCardArtImage>> images;
  images.push_back(std::move(credit_card_art_image));

  personal_data_->OnCardArtImagesFetched(std::move(images));

  gfx::Image* actual_image = personal_data_->GetCreditCardArtImageForUrl(
      GURL("https://www.example.com"));
  ASSERT_TRUE(actual_image);
  EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, *actual_image));

  // TODO(crbug.com/1284788): Look into integrating with PersonalDataManagerMock
  // and checking that PersonalDataManager::FetchImagesForUrls() does not get
  // triggered when PersonalDataManager::GetCachedCardArtImageForUrl() is
  // called.
  gfx::Image* cached_image = personal_data_->GetCachedCardArtImageForUrl(
      GURL("https://www.example.com"));
  ASSERT_TRUE(cached_image);
  EXPECT_TRUE(gfx::test::AreImagesEqual(expected_image, *cached_image));
}

TEST_F(PersonalDataManagerTest,
       TestNoImageFetchingAttemptForCardsWithInvalidCardArtUrls) {
  base::HistogramTester histogram_tester;

  gfx::Image* actual_image =
      personal_data_->GetCreditCardArtImageForUrl(GURL());
  EXPECT_FALSE(actual_image);
  EXPECT_EQ(0, histogram_tester.GetTotalSum("Autofill.ImageFetcher.Result"));
}

TEST_F(PersonalDataManagerMockTest, ProcessCardArtUrlChanges) {
  CreditCard card = test::GetFullServerCard();
  card.set_server_id("card_server_id");
  personal_data_->AddFullServerCreditCardForTesting(card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  card.set_server_id("card_server_id");
  card.set_card_art_url(GURL("https://www.example.com/card1"));
  std::vector<GURL> updated_urls;
  updated_urls.emplace_back("https://www.example.com/card1");

  personal_data_->AddFullServerCreditCardForTesting(card);
  WaitForFetchImagesForUrls();

  card.set_card_art_url(GURL("https://www.example.com/card2"));
  updated_urls.clear();
  updated_urls.emplace_back("https://www.example.com/card2");

  personal_data_->AddFullServerCreditCardForTesting(card);
  WaitForFetchImagesForUrls();
}
#endif

TEST_F(PersonalDataManagerTest, UpdateUnverifiedCreditCards) {
  // Start with unverified data.
  CreditCard credit_card = test::GetCreditCard();
  EXPECT_FALSE(credit_card.IsVerified());

  // Add the data to the database.
  personal_data_->AddCreditCard(credit_card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_THAT(personal_data_->GetCreditCards(),
              testing::UnorderedElementsAre(Pointee(credit_card)));

  // Try to update with just the origin changed.
  CreditCard original_credit_card(credit_card);
  credit_card.set_origin(kSettingsOrigin);
  EXPECT_TRUE(credit_card.IsVerified());
  personal_data_->UpdateCreditCard(credit_card);

  // Credit Card origin should not be overwritten.
  EXPECT_THAT(personal_data_->GetCreditCards(),
              testing::UnorderedElementsAre(Pointee(original_credit_card)));

  // Try to update with data changed as well.
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Joe");
  personal_data_->UpdateCreditCard(credit_card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_THAT(personal_data_->GetCreditCards(),
              testing::UnorderedElementsAre(Pointee(credit_card)));
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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

TEST_F(PersonalDataManagerTest, SetUniqueCreditCardLabels) {
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card0.SetRawInfo(CREDIT_CARD_NAME_FULL, u"John");
  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card1.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul");
  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card2.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Ringo");
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card3.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Other");
  CreditCard credit_card4(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card4.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Ozzy");
  CreditCard credit_card5(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  credit_card5.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Dio");

  // Add the test credit cards to the database.
  personal_data_->AddCreditCard(credit_card0);
  personal_data_->AddCreditCard(credit_card1);
  personal_data_->AddCreditCard(credit_card2);
  personal_data_->AddCreditCard(credit_card3);
  personal_data_->AddCreditCard(credit_card4);
  personal_data_->AddCreditCard(credit_card5);

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  std::vector<CreditCard*> cards;
  cards.push_back(&credit_card0);
  cards.push_back(&credit_card1);
  cards.push_back(&credit_card2);
  cards.push_back(&credit_card3);
  cards.push_back(&credit_card4);
  cards.push_back(&credit_card5);
  ExpectSameElements(cards, personal_data_->GetCreditCards());
}

TEST_F(PersonalDataManagerTest, SetEmptyCreditCard) {
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card0, "", "", "", "", "");

  // Add the empty credit card to the database.
  personal_data_->AddCreditCard(credit_card0);

  // Note: no refresh here.

  // Reset the PersonalDataManager.  This tests that the personal data was saved
  // to the web database, and that we can load the credit cards from the web
  // database.
  ResetPersonalDataManager();

  // Verify that we've loaded the credit cards from the web database.
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());
}

TEST_F(PersonalDataManagerTest, SaveCardLocallyIfNewWithNewCard) {
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Sunraku Emul",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");

  EXPECT_EQ(0U, personal_data_->GetCreditCards().size());

  // Add the credit card to the database.
  bool is_saved = personal_data_->SaveCardLocallyIfNew(credit_card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Expect that the credit card was saved.
  EXPECT_TRUE(is_saved);
  std::vector<CreditCard> saved_credit_cards;
  for (auto* result : personal_data_->GetCreditCards()) {
    saved_credit_cards.push_back(*result);
  }

  EXPECT_THAT(saved_credit_cards, ElementsAre(credit_card));
}

TEST_F(PersonalDataManagerTest, SaveCardLocallyIfNewWithExistingCard) {
  const char* credit_card_number = "4111 1111 1111 1111" /* Visa */;
  CreditCard credit_card(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                         kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Sunraku Emul", credit_card_number,
                          "01", "2999", "");

  // Add the credit card to the database.
  personal_data_->AddCreditCard(credit_card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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

  EXPECT_THAT(saved_credit_cards, ElementsAre(credit_card));
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  CreditCard new_verified_card = credit_card;
  new_verified_card.set_guid(
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  new_verified_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"B. Small");
  EXPECT_TRUE(new_verified_card.IsVerified());

  personal_data_->OnAcceptedLocalCreditCardSave(new_verified_card);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Expect that the saved credit card is updated.
  const std::vector<CreditCard*>& results = personal_data_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(u"B. Small", results[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
}

// Tests that GetAutofillOffers returns all available offers.
TEST_F(PersonalDataManagerTest, GetAutofillOffers) {
  // Add two card-linked offers and one promo code offer.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  AddOfferDataForTest(test::GetCardLinkedOfferData2());
  AddOfferDataForTest(test::GetPromoCodeOfferData());

  // Should return all three.
  EXPECT_EQ(3U, personal_data_->GetAutofillOffers().size());
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

// Tests that GetActiveAutofillPromoCodeOffersForOrigin returns only active and
// site-relevant promo code offers.
TEST_F(PersonalDataManagerTest, GetActiveAutofillPromoCodeOffersForOrigin) {
  // Card-linked offers should not be returned.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  // Expired promo code offers should not be returned.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com"), /*is_expired=*/true));
  // Active promo code offers should be returned.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.example.com"), /*is_expired=*/false));
  // Active promo code offers for a different site should not be returned.
  AddOfferDataForTest(test::GetPromoCodeOfferData(
      /*origin=*/GURL("http://www.some-other-merchant.com"),
      /*is_expired=*/false));

  // Only the active offer for example.com should be returned.
  EXPECT_EQ(1U, personal_data_
                    ->GetActiveAutofillPromoCodeOffersForOrigin(
                        GURL("http://www.example.com"))
                    .size());
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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

  // Since there are no profiles set, the country code supplied buy variations
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

  // Now a profile is set and the correct caching of the country code is
  // verified once more.
  AddProfileToPersonalDataManager(test::GetFullProfile());
  actual_country_code = personal_data_->GetDefaultCountryCodeForNewAddress();
  EXPECT_EQ(actual_country_code, expected_country_code);
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  ASSERT_FALSE(personal_data_->IsServerCard(&local_card));
}

// Test that local credit cards are ordered as expected.
TEST_F(PersonalDataManagerTest, GetCreditCardsToSuggest_LocalCardsRanking) {
  SetUpReferenceLocalCreditCards();

  // Sublabel is card number when filling name (exact format depends on
  // the platform, but the last 4 digits should appear).
  std::vector<CreditCard*> card_to_suggest =
      personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(3U, card_to_suggest.size());

  // Ordered as expected.
  EXPECT_EQ(u"John Dillinger",
            card_to_suggest[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Clyde Barrow",
            card_to_suggest[1]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Bonnie Parker",
            card_to_suggest[2]->GetRawInfo(CREDIT_CARD_NAME_FULL));
}

// Test that local and server cards are ordered as expected.
TEST_F(PersonalDataManagerTest,
       GetCreditCardsToSuggest_LocalAndServerCardsRanking) {
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

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(5U, personal_data_->GetCreditCards().size());

  std::vector<CreditCard*> card_to_suggest =
      personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(5U, card_to_suggest.size());

  // All cards should be ordered as expected.
  EXPECT_EQ(u"Jesse James",
            card_to_suggest[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"John Dillinger",
            card_to_suggest[1]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Clyde Barrow",
            card_to_suggest[2]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Emmet Dalton",
            card_to_suggest[3]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Bonnie Parker",
            card_to_suggest[4]->GetRawInfo(CREDIT_CARD_NAME_FULL));
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Disable Credit card autofill.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

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

// Tests the suggestions of duplicate local and server credit cards.
TEST_F(PersonalDataManagerTest, GetCreditCardsToSuggest_ServerDuplicates) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillSuggestServerCardInsteadOfLocalCard);
  SetUpReferenceLocalCreditCards();

  // Add some server cards. If there are local dupes, the locals should be
  // hidden.
  std::vector<CreditCard> server_cards;
  // This server card matches a local card, except the local card is missing the
  // number. This should count as a dupe and thus not be shown in the
  // suggestions since the locally saved card takes precedence.
  server_cards.emplace_back(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "3456" /* Visa */, "01", "2999", "1");
  server_cards.back().set_use_count(2);
  server_cards.back().set_use_date(AutofillClock::Now() - base::Days(15));
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(4U, personal_data_->GetCreditCards().size());

  std::vector<CreditCard*> card_to_suggest =
      personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(3U, card_to_suggest.size());
  EXPECT_EQ(u"John Dillinger",
            card_to_suggest[0]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Clyde Barrow",
            card_to_suggest[1]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(u"Bonnie Parker",
            card_to_suggest[2]->GetRawInfo(CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(CreditCard::RecordType::kLocalCard,
            card_to_suggest[0]->record_type());
  EXPECT_EQ(CreditCard::RecordType::kLocalCard,
            card_to_suggest[1]->record_type());
  EXPECT_EQ(CreditCard::RecordType::kLocalCard,
            card_to_suggest[2]->record_type());
}

// Tests that a full server card can be a dupe of more than one local card.
TEST_F(PersonalDataManagerTest,
       GetCreditCardsToSuggest_ServerCardDuplicateOfMultipleLocalCards) {
  SetUpReferenceLocalCreditCards();

  // Add a duplicate server card.
  std::vector<CreditCard> server_cards;
  // This unmasked server card is an exact dupe of a local card. Therefore only
  // the local card should appear in the suggestions.
  server_cards.emplace_back(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");

  SetServerCards(server_cards);

  // Make sure everything is set up correctly.
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_EQ(4U, personal_data_->GetCreditCards().size());

  std::vector<CreditCard*> card_to_suggest =
      personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(3U, card_to_suggest.size());

  // Add a second dupe local card to make sure a full server card can be a dupe
  // of more than one local card.
  CreditCard credit_card3("4141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Clyde Barrow", "", "04", "", "");
  personal_data_->AddCreditCard(credit_card3);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  card_to_suggest = personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(3U, card_to_suggest.size());
}

// Tests that only the full server card is kept when deduping with a local
// duplicate of it.
TEST_F(PersonalDataManagerTest,
       DedupeCreditCardToSuggest_FullServerShadowsLocal) {
  std::list<CreditCard*> credit_cards;

  // Create 3 different local credit cards.
  CreditCard local_card("287151C8-6AB1-487C-9095-28E80BE5DA15",
                        test::kEmptyOrigin);
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  local_card.set_use_count(3);
  local_card.set_use_date(AutofillClock::Now() - base::Days(1));
  credit_cards.push_back(&local_card);

  // Create a full server card that is a duplicate of one of the local cards.
  CreditCard full_server_card(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(AutofillClock::Now() - base::Days(15));
  credit_cards.push_back(&full_server_card);

  PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
  ASSERT_EQ(1U, credit_cards.size());

  const CreditCard* deduped_card = credit_cards.front();
  EXPECT_TRUE(*deduped_card == full_server_card);
}

// Tests that only the local card is kept when deduping with a masked server
// duplicate of it or vice-versa. This is checked based on the value assigned
// during the for loop.
TEST_F(PersonalDataManagerTest,
       DedupeCreditCardToSuggest_BothLocalAndServerShadowsMaskedInTurns) {
  for (bool is_dedupe_experiment_active : {true, false}) {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(
        features::kAutofillSuggestServerCardInsteadOfLocalCard,
        is_dedupe_experiment_active);
    std::list<CreditCard*> credit_cards;

    CreditCard local_card("1141084B-72D7-4B73-90CF-3D6AC154673B",
                          test::kEmptyOrigin);
    test::SetCreditCardInfo(&local_card, "Homer Simpson",
                            "4234567890123456" /* Visa */, "01", "2999", "1");
    credit_cards.push_back(&local_card);

    // Create a masked server card that is a duplicate of a local card.
    CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
    test::SetCreditCardInfo(&masked_card, "Homer Simpson", "3456" /* Visa */,
                            "01", "2999", "1");
    masked_card.SetNetworkForMaskedCard(kVisaCard);
    credit_cards.push_back(&masked_card);

    PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
    ASSERT_EQ(1U, credit_cards.size());

    const CreditCard* deduped_card = credit_cards.front();
    if (is_dedupe_experiment_active) {
      EXPECT_EQ(*deduped_card, masked_card);
    } else {
      EXPECT_EQ(*deduped_card, local_card);
    }
  }
}

// Tests that identical full server and masked credit cards are not deduped.
TEST_F(PersonalDataManagerTest, DedupeCreditCardToSuggest_FullServerAndMasked) {
  std::list<CreditCard*> credit_cards;

  // Create a full server card that is a duplicate of one of the local cards.
  CreditCard full_server_card(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "4234567890123456" /* Visa */, "01", "2999", "1");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(AutofillClock::Now() - base::Days(15));
  credit_cards.push_back(&full_server_card);

  // Create a masked server card that is a duplicate of a local card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "3456" /* Visa */,
                          "01", "2999", "1");
  masked_card.set_use_count(2);
  masked_card.set_use_date(AutofillClock::Now() - base::Days(15));
  masked_card.SetNetworkForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
  EXPECT_EQ(2U, credit_cards.size());
}

// Tests that different local, masked, and full server credit cards are not
// deduped.
TEST_F(PersonalDataManagerTest, DedupeCreditCardToSuggest_DifferentCards) {
  std::list<CreditCard*> credit_cards;

  CreditCard local_card("002149C1-EE28-4213-A3B9-DA243FFF021B",
                        test::kEmptyOrigin);
  local_card.set_use_count(1);
  local_card.set_use_date(AutofillClock::Now() - base::Days(1));
  test::SetCreditCardInfo(&local_card, "Homer Simpson",
                          "5105105105105100" /* Mastercard */, "", "", "");
  credit_cards.push_back(&local_card);

  // Create a masked server card that is different from the local card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "b456");
  test::SetCreditCardInfo(&masked_card, "Homer Simpson", "0005", "12", "2999",
                          "1");
  masked_card.set_use_count(3);
  masked_card.set_use_date(AutofillClock::Now() - base::Days(15));
  // credit_card4.SetNetworkForMaskedCard(kVisaCard);
  credit_cards.push_back(&masked_card);

  // Create a full server card that is slightly different of the two other
  // cards.
  CreditCard full_server_card(CreditCard::RecordType::kFullServerCard, "c789");
  test::SetCreditCardInfo(&full_server_card, "Homer Simpson",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  full_server_card.set_use_count(1);
  full_server_card.set_use_date(AutofillClock::Now() - base::Days(15));
  credit_cards.push_back(&full_server_card);

  PersonalDataManager::DedupeCreditCardToSuggest(&credit_cards);
  EXPECT_EQ(3U, credit_cards.size());
}

TEST_F(PersonalDataManagerTest, RecordUseOfCard) {
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  CreditCard card = test::GetCreditCard();
  ASSERT_EQ(card.use_count(), 1u);
  ASSERT_EQ(card.use_date(), kArbitraryTime);
  ASSERT_EQ(card.modification_date(), kArbitraryTime);
  personal_data_->AddCreditCard(card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  test_clock.SetNow(kSomeLaterTime);
  personal_data_->RecordUseOf(&card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  CreditCard* pdm_card = personal_data_->GetCreditCardByGUID(card.guid());
  ASSERT_TRUE(pdm_card);
  EXPECT_EQ(pdm_card->use_count(), 2u);
  EXPECT_EQ(pdm_card->use_date(), kSomeLaterTime);
  EXPECT_EQ(pdm_card->modification_date(), kArbitraryTime);
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

TEST_F(PersonalDataManagerTest, DeleteLocalCreditCards) {
  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Alice",
                          "378282246310005" /* American Express */, "04",
                          "2020", "1");
  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "Ben",
                          "378282246310006" /* American Express */, "04",
                          "2021", "1");
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card3, "Clyde",
                          "5105105105105100" /* Mastercard */, "04", "2022",
                          "1");
  std::vector<CreditCard> cards;
  cards.push_back(credit_card1);
  cards.push_back(credit_card2);

  personal_data_->AddCreditCard(credit_card1);
  personal_data_->AddCreditCard(credit_card2);
  personal_data_->AddCreditCard(credit_card3);

  personal_data_->DeleteLocalCreditCards(cards);

  // Wait for the data to be refreshed.
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  std::unordered_set<std::u16string> expectedToRemain = {u"Clyde"};
  for (auto* card : personal_data_->GetCreditCards()) {
    EXPECT_NE(expectedToRemain.end(),
              expectedToRemain.find(card->GetRawInfo(CREDIT_CARD_NAME_FULL)));
  }
}

TEST_F(PersonalDataManagerTest, DeleteAllLocalCreditCards) {
  SetUpReferenceLocalCreditCards();

  // Expect 3 local credit cards.
  EXPECT_EQ(3U, personal_data_->GetLocalCreditCards().size());

  personal_data_->DeleteAllLocalCreditCards();

  // Wait for the data to be refreshed.
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Expect the local credit cards to have been deleted.
  EXPECT_EQ(0U, personal_data_->GetLocalCreditCards().size());
}

TEST_F(PersonalDataManagerTest, LogStoredCreditCardMetrics) {
  ASSERT_EQ(0U, personal_data_->GetCreditCards().size());

  // Helper timestamps for setting up the test data.
  base::Time now = AutofillClock::Now();
  base::Time one_month_ago = now - base::Days(30);
  base::Time::Exploded one_month_ago_exploded;
  one_month_ago.LocalExplode(&one_month_ago_exploded);

  std::vector<CreditCard> server_cards;
  server_cards.reserve(10);

  // Create in-use and in-disuse cards of each record type.
  const std::vector<CreditCard::RecordType> record_types{
      CreditCard::RecordType::kLocalCard,
      CreditCard::RecordType::kMaskedServerCard};
  for (auto record_type : record_types) {
    // Create a card that's still in active use.
    CreditCard card_in_use = test::GetRandomCreditCard(record_type);
    card_in_use.set_use_date(now - base::Days(30));
    card_in_use.set_use_count(10);

    // Create a card that's not in active use.
    CreditCard card_in_disuse = test::GetRandomCreditCard(record_type);
    card_in_disuse.SetExpirationYear(one_month_ago_exploded.year);
    card_in_disuse.SetExpirationMonth(one_month_ago_exploded.month);
    card_in_disuse.set_use_date(now - base::Days(200));
    card_in_disuse.set_use_count(10);

    // Add the cards to the personal data manager in the appropriate way.
    if (record_type == CreditCard::RecordType::kLocalCard) {
      personal_data_->AddCreditCard(card_in_use);
      personal_data_->AddCreditCard(card_in_disuse);
    } else {
      server_cards.push_back(std::move(card_in_use));
      server_cards.push_back(std::move(card_in_disuse));
    }
  }

  // Sets the virtual card enrollment state for the first server card.
  server_cards[0].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  server_cards[0].set_card_art_url(GURL("https://www.example.com/image1"));

  SetServerCards(server_cards);

  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ASSERT_EQ(4U, personal_data_->GetCreditCards().size());

  // Reload the database, which will log the stored profile counts.
  base::HistogramTester histogram_tester;
  ResetPersonalDataManager();

  EXPECT_EQ(personal_data_->GetServerCardWithArtImageCount(), 1U);

  ASSERT_EQ(4U, personal_data_->GetCreditCards().size());

  // Validate the basic count metrics for both local and server cards. Deep
  // validation of the metrics is done in:
  //    AutofillMetricsTest::LogStoredCreditCardMetrics
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Local", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Server", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.Masked", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.Unmasked", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount", 4, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Local", 2,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Server", 2,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.Masked", 2, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.Unmasked", 0, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.WithVirtualCardMetadata", 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.WithCardArtImage", 1, 1);
}

// Test that setting a null sync service returns only local credit cards.
TEST_F(PersonalDataManagerTest, GetCreditCards_NoSyncService) {
  base::HistogramTester histogram_tester;
  SetUpTwoCardTypes();

  // Set no sync service.
  personal_data_->SetSyncServiceForTest(nullptr);
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

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
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged());
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

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

// Sanity check that the mode where we use the regular, persistent storage for
// cards still works.
TEST_F(PersonalDataManagerTest, UsePersistentServerStorage) {
  ASSERT_TRUE(identity_test_env_.identity_manager()->HasPrimaryAccount(
      signin::ConsentLevel::kSync));
  ASSERT_TRUE(sync_service_.HasSyncConsent());
  SetUpTwoCardTypes();

  EXPECT_EQ(2U, personal_data_->GetCreditCards().size());
  EXPECT_EQ(2U, personal_data_->GetCreditCardsToSuggest().size());
  EXPECT_EQ(1U, personal_data_->GetLocalCreditCards().size());
  EXPECT_EQ(1U, personal_data_->GetServerCreditCards().size());
}

// Verify that PDM can switch at runtime between the different storages.
TEST_F(PersonalDataManagerSyncTransportModeTest, SwitchServerStorages) {
  // Start with account storage.
  SetUpTwoCardTypes();

  // Check that we do have a server card, as expected.
  ASSERT_EQ(1U, personal_data_->GetServerCreditCards().size());

  // Switch to persistent storage.
  sync_service_.SetHasSyncConsent(true);
  personal_data_->OnStateChanged(&sync_service_);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_EQ(0U, personal_data_->GetServerCreditCards().size());

  // Add a new card to the persistent storage.
  CreditCard server_card;
  test::SetCreditCardInfo(&server_card, "Server Card",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  server_card.set_guid("00000000-0000-0000-0000-000000000007");
  server_card.set_record_type(CreditCard::RecordType::kFullServerCard);
  server_card.set_server_id("server_id");
  // TODO(crbug.com/1497734): Switch to an appropriate setter for masked
  // cards, as full cards have been removed.
  personal_data_->AddFullServerCreditCardForTesting(server_card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  EXPECT_EQ(1U, personal_data_->GetServerCreditCards().size());

  // Switch back to the account storage, and verify that we are back to the
  // original card.
  sync_service_.SetHasSyncConsent(false);
  personal_data_->OnStateChanged(&sync_service_);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ASSERT_EQ(1U, personal_data_->GetServerCreditCards().size());
  EXPECT_EQ(u"3456", personal_data_->GetServerCreditCards()[0]->number());
}

// Sanity check that the mode where we use the regular, persistent storage for
// cards still works.
TEST_F(PersonalDataManagerSyncTransportModeTest,
       UseCorrectStorageForDifferentCards) {
  // Add a server card.
  CreditCard server_card;
  test::SetCreditCardInfo(&server_card, "Server Card",
                          "4234567890123456",  // Visa
                          "04", "2999", "1");
  server_card.set_guid("00000000-0000-0000-0000-000000000007");
  server_card.set_record_type(CreditCard::RecordType::kFullServerCard);
  server_card.set_server_id("server_id");
  personal_data_->AddFullServerCreditCardForTesting(server_card);

  // Set server card metadata.
  server_card.set_use_count(15);
  personal_data_->UpdateServerCardsMetadata({server_card});

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Expect that the server card is stored in the account autofill table.
  std::vector<std::unique_ptr<CreditCard>> cards;
  account_autofill_table_->GetServerCreditCards(cards);
  EXPECT_EQ(1U, cards.size());
  EXPECT_EQ(server_card.LastFourDigits(), cards[0]->LastFourDigits());

  // Add a local card.
  CreditCard local_card;
  test::SetCreditCardInfo(&local_card, "Freddy Mercury",
                          "4234567890123463",  // Visa
                          "08", "2999", "1");
  local_card.set_guid("00000000-0000-0000-0000-000000000009");
  local_card.set_record_type(CreditCard::RecordType::kLocalCard);
  local_card.set_use_date(AutofillClock::Now() - base::Days(5));
  personal_data_->AddCreditCard(local_card);

  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Expect that the local card is stored in the profile autofill table.
  profile_autofill_table_->GetCreditCards(&cards);
  EXPECT_EQ(1U, cards.size());
  EXPECT_EQ(local_card.LastFourDigits(), cards[0]->LastFourDigits());
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

TEST_F(PersonalDataManagerTest, SaveProfileMigrationStrikes) {
  EXPECT_FALSE(personal_data_->IsProfileMigrationBlocked(kGuid));

  personal_data_->AddStrikeToBlockProfileMigration(kGuid);
  EXPECT_FALSE(personal_data_->IsProfileMigrationBlocked(kGuid));

  personal_data_->AddStrikeToBlockProfileMigration(kGuid);
  EXPECT_FALSE(personal_data_->IsProfileMigrationBlocked(kGuid));

  // After the third strike, the guid should be blocked.
  personal_data_->AddStrikeToBlockProfileMigration(kGuid);
  EXPECT_TRUE(personal_data_->IsProfileMigrationBlocked(kGuid));

  // Until the strikes are removed again.
  personal_data_->RemoveStrikesToBlockProfileMigration(kGuid);
  EXPECT_FALSE(personal_data_->IsProfileMigrationBlocked(kGuid));

  // `AddMaxStrikesToBlockProfileMigration()` should add sufficiently many
  // strikes.
  personal_data_->AddMaxStrikesToBlockProfileMigration(kGuid);
  EXPECT_TRUE(personal_data_->IsProfileMigrationBlocked(kGuid));
}

TEST_F(PersonalDataManagerTest, SaveProfileUpdateStrikes) {
  EXPECT_FALSE(personal_data_->IsProfileUpdateBlocked(kGuid));

  personal_data_->AddStrikeToBlockProfileUpdate(kGuid);
  EXPECT_FALSE(personal_data_->IsProfileUpdateBlocked(kGuid));

  personal_data_->AddStrikeToBlockProfileUpdate(kGuid);
  EXPECT_FALSE(personal_data_->IsProfileUpdateBlocked(kGuid));

  // After the third strike, the guid should be blocked.
  personal_data_->AddStrikeToBlockProfileUpdate(kGuid);
  EXPECT_TRUE(personal_data_->IsProfileUpdateBlocked(kGuid));

  // Until the strikes are removed again.
  personal_data_->RemoveStrikesToBlockProfileUpdate(kGuid);
  EXPECT_FALSE(personal_data_->IsProfileUpdateBlocked(kGuid));
}

TEST_F(PersonalDataManagerTest, SaveProfileSaveStrikes) {
  GURL domain("https://www.block.me/index.html");

  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(domain));

  personal_data_->AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(domain));

  personal_data_->AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(domain));

  // After the third strike, the domain should be blocked.
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(domain));

  // Until the strikes are removed again.
  personal_data_->RemoveStrikesToBlockNewProfileImportForDomain(domain);
  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(domain));
}

TEST_F(PersonalDataManagerTest, ClearFullBrowsingHistory) {
  GURL domain("https://www.block.me/index.html");

  personal_data_->AddStrikeToBlockNewProfileImportForDomain(domain);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(domain);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(domain);
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(domain));

  history::DeletionInfo deletion_info = history::DeletionInfo::ForAllHistory();

  personal_data_->OnURLsDeleted(/*history_service=*/nullptr, deletion_info);

  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(domain));
}

TEST_F(PersonalDataManagerTest, ClearUrlsFromBrowsingHistory) {
  GURL first_url("https://www.block.me/index.html");
  GURL second_url("https://www.block.too/index.html");

  // Add strikes to block both domains.
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(first_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(first_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(first_url);
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(first_url));

  personal_data_->AddStrikeToBlockNewProfileImportForDomain(second_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(second_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(second_url);
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(second_url));

  history::URLRows deleted_urls = {history::URLRow(first_url)};

  history::DeletionInfo deletion_info =
      history::DeletionInfo::ForUrls(deleted_urls, {});

  personal_data_->OnURLsDeleted(/*history_service=*/nullptr, deletion_info);

  // The strikes for `domain` should be deleted, but the strikes for
  // `another_domain` should not.
  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(first_url));
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(second_url));
}

TEST_F(PersonalDataManagerTest, ClearUrlsFromBrowsingHistoryInTimeRange) {
  GURL first_url("https://www.block.me/index.html");
  GURL second_url("https://www.block.too/index.html");

  TestAutofillClock test_clock;

  // Add strikes to block both domains.
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(first_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(first_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(first_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(second_url);
  personal_data_->AddStrikeToBlockNewProfileImportForDomain(second_url);
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(first_url));

  test_clock.Advance(base::Hours(1));
  base::Time end_of_deletion = AutofillClock::Now();
  test_clock.Advance(base::Hours(1));

  personal_data_->AddStrikeToBlockNewProfileImportForDomain(second_url);
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(second_url));

  history::URLRows deleted_urls = {history::URLRow(first_url),
                                   history::URLRow(second_url)};

  history::DeletionInfo deletion_info(
      history::DeletionTimeRange(base::Time::Min(), end_of_deletion), false,
      deleted_urls, {},
      std::make_optional<std::set<GURL>>({first_url, second_url}));

  personal_data_->OnURLsDeleted(/*history_service=*/nullptr, deletion_info);

  // The strikes for `first_url` should be deleted because the strikes have been
  // added within the deletion time range.
  EXPECT_FALSE(personal_data_->IsNewProfileImportBlockedForDomain(first_url));
  // The last strike for 'second_url' was collected after the deletion time
  // range and therefore, the blocking should prevail.
  EXPECT_TRUE(personal_data_->IsNewProfileImportBlockedForDomain(second_url));
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-set some server cards. Check that the function now returns true.
  SetServerCards(server_cards);
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

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
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_FALSE(personal_data_->ShouldShowCardsFromAccountOption());

  // Re-set some server cards. Check that the function still returns false.
  SetServerCards(server_cards);
  personal_data_->Refresh();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
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

TEST_F(PersonalDataManagerTest, ClearAllCvcs) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  // Add a server card and its CVC.
  CreditCard server_card = test::GetMaskedServerCard();
  const std::u16string server_cvc = u"111";
  SetServerCards({server_card});
  personal_data_->AddServerCvc(server_card.instrument_id(), server_cvc);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  // Add a local card and its CVC.
  CreditCard local_card = test::GetCreditCard();
  const std::u16string local_cvc = u"999";
  local_card.set_cvc(local_cvc);
  personal_data_->AddCreditCard(local_card);
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();

  ASSERT_EQ(personal_data_->GetLocalCreditCards().size(), 1U);
  ASSERT_EQ(personal_data_->GetServerCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetServerCreditCards()[0]->cvc(), server_cvc);
  EXPECT_EQ(personal_data_->GetLocalCreditCards()[0]->cvc(), local_cvc);

  // Clear out all the CVCs (local + server).
  personal_data_->ClearLocalCvcs();
  personal_data_->ClearServerCvcs();
  PersonalDataProfileTaskWaiter(*personal_data_).Wait();
  EXPECT_TRUE(personal_data_->GetServerCreditCards()[0]->cvc().empty());
  EXPECT_TRUE(personal_data_->GetLocalCreditCards()[0]->cvc().empty());
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

// Tests that benefit getters return expected result for active benefits.
TEST_F(PersonalDataManagerTest, GetActiveCreditCardBenefits) {
  // Add active benefits.
  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_flat_rate_benefit =
          flat_rate_benefit.linked_card_instrument_id();
  personal_data_->AddCreditCardBenefitForTest(std::move(flat_rate_benefit));

  CreditCardCategoryBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_category_benefit =
          category_benefit.linked_card_instrument_id();
  const CreditCardCategoryBenefit::BenefitCategory
      benefit_category_for_category_benefit =
          category_benefit.benefit_category();
  personal_data_->AddCreditCardBenefitForTest(std::move(category_benefit));

  CreditCardMerchantBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_merchant_benefit =
          merchant_benefit.linked_card_instrument_id();
  url::Origin merchant_origin_for_merchant_benefit =
      *merchant_benefit.merchant_domains().begin();
  personal_data_->AddCreditCardBenefitForTest(std::move(merchant_benefit));

  // Match getter results with the search criteria.
  EXPECT_TRUE(personal_data_->IsAutofillPaymentMethodsEnabled());
  EXPECT_EQ(personal_data_
                ->GetFlatRateBenefitByInstrumentId(
                    instrument_id_for_flat_rate_benefit)
                ->linked_card_instrument_id(),
            instrument_id_for_flat_rate_benefit);

  std::optional<CreditCardCategoryBenefit> category_benefit_result =
      personal_data_->GetCategoryBenefitByInstrumentIdAndCategory(
          instrument_id_for_category_benefit,
          benefit_category_for_category_benefit);
  EXPECT_EQ(category_benefit_result->linked_card_instrument_id(),
            instrument_id_for_category_benefit);
  EXPECT_EQ(category_benefit_result->benefit_category(),
            benefit_category_for_category_benefit);

  std::optional<CreditCardMerchantBenefit> merchant_benefit_result =
      personal_data_->GetMerchantBenefitByInstrumentIdAndOrigin(
          instrument_id_for_merchant_benefit,
          merchant_origin_for_merchant_benefit);
  EXPECT_EQ(merchant_benefit_result->linked_card_instrument_id(),
            instrument_id_for_merchant_benefit);
  EXPECT_TRUE(merchant_benefit_result->merchant_domains().contains(
      merchant_origin_for_merchant_benefit));

  // Disable autofill credit card pref. Check that no benefits are returned.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  EXPECT_FALSE(personal_data_->GetFlatRateBenefitByInstrumentId(
      instrument_id_for_flat_rate_benefit));
  EXPECT_FALSE(personal_data_->GetCategoryBenefitByInstrumentIdAndCategory(
      instrument_id_for_category_benefit,
      benefit_category_for_category_benefit));
  EXPECT_FALSE(personal_data_->GetMerchantBenefitByInstrumentIdAndOrigin(
      instrument_id_for_merchant_benefit,
      merchant_origin_for_merchant_benefit));
}

// Tests benefit getters will not return inactive benefits.
TEST_F(PersonalDataManagerTest, GetInactiveCreditCardBenefits) {
  // Add inactive benefits.
  base::Time future_time = AutofillClock::Now() + base::Days(5);

  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  test_api(flat_rate_benefit).SetStartTimeForTesting(future_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_flat_rate_benefit =
          flat_rate_benefit.linked_card_instrument_id();
  personal_data_->AddCreditCardBenefitForTest(std::move(flat_rate_benefit));

  CreditCardCategoryBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  test_api(category_benefit).SetStartTimeForTesting(future_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_category_benefit =
          category_benefit.linked_card_instrument_id();
  const CreditCardCategoryBenefit::BenefitCategory
      benefit_category_for_category_benefit =
          category_benefit.benefit_category();
  personal_data_->AddCreditCardBenefitForTest(std::move(category_benefit));

  CreditCardMerchantBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  test_api(merchant_benefit).SetStartTimeForTesting(future_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_merchant_benefit =
          merchant_benefit.linked_card_instrument_id();
  url::Origin merchant_origin_for_merchant_benefit =
      *merchant_benefit.merchant_domains().begin();
  personal_data_->AddCreditCardBenefitForTest(std::move(merchant_benefit));

  // Should not return any benefits as no benefit is currently active.
  EXPECT_FALSE(personal_data_->GetFlatRateBenefitByInstrumentId(
      instrument_id_for_flat_rate_benefit));
  EXPECT_FALSE(personal_data_->GetCategoryBenefitByInstrumentIdAndCategory(
      instrument_id_for_category_benefit,
      benefit_category_for_category_benefit));
  EXPECT_FALSE(personal_data_->GetMerchantBenefitByInstrumentIdAndOrigin(
      instrument_id_for_merchant_benefit,
      merchant_origin_for_merchant_benefit));
}

// Tests benefit getters will not return expired benefits.
TEST_F(PersonalDataManagerTest, GetExpiredCreditCardBenefits) {
  // Add Expired benefits.
  base::Time expired_time = AutofillClock::Now() - base::Days(5);

  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  test_api(flat_rate_benefit).SetEndTimeForTesting(expired_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_flat_rate_benefit =
          flat_rate_benefit.linked_card_instrument_id();
  personal_data_->AddCreditCardBenefitForTest(std::move(flat_rate_benefit));

  CreditCardCategoryBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  test_api(category_benefit).SetEndTimeForTesting(expired_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_category_benefit =
          category_benefit.linked_card_instrument_id();
  const CreditCardCategoryBenefit::BenefitCategory
      benefit_category_for_category_benefit =
          category_benefit.benefit_category();
  personal_data_->AddCreditCardBenefitForTest(std::move(category_benefit));

  CreditCardMerchantBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  test_api(merchant_benefit).SetEndTimeForTesting(expired_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_merchant_benefit =
          merchant_benefit.linked_card_instrument_id();
  url::Origin merchant_origin_for_merchant_benefit =
      *merchant_benefit.merchant_domains().begin();
  personal_data_->AddCreditCardBenefitForTest(std::move(merchant_benefit));

  // Should not return any benefits as all of the benefits are expired.
  EXPECT_FALSE(personal_data_->GetFlatRateBenefitByInstrumentId(
      instrument_id_for_flat_rate_benefit));
  EXPECT_FALSE(personal_data_->GetCategoryBenefitByInstrumentIdAndCategory(
      instrument_id_for_category_benefit,
      benefit_category_for_category_benefit));
  EXPECT_FALSE(personal_data_->GetMerchantBenefitByInstrumentIdAndOrigin(
      instrument_id_for_merchant_benefit,
      merchant_origin_for_merchant_benefit));
}

}  // namespace autofill
