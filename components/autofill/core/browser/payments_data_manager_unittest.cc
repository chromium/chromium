// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments_data_manager.h"

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
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
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/credit_card_art_image.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit_test_api.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/payments_data_manager_test_api.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_test_base.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/ui/autofill_image_fetcher_base.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
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

namespace autofill {

namespace {

using testing::Pointee;

const base::Time kArbitraryTime = base::Time::FromSecondsSinceUnixEpoch(25);
const base::Time kSomeLaterTime = base::Time::FromSecondsSinceUnixEpoch(1000);

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

class PaymentsDataManagerHelper : public PersonalDataManagerTestBase {
 protected:
  PaymentsDataManagerHelper() = default;

  virtual ~PaymentsDataManagerHelper() {
    if (personal_data_) {
      personal_data_->Shutdown();
    }
    personal_data_.reset();
  }

  void ResetPersonalDataManager(bool use_sync_transport_mode = false) {
    if (personal_data_) {
      personal_data_->Shutdown();
    }
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

  void RemoveByGUIDFromPersonalDataManager(const std::string& guid) {
    PersonalDataChangedWaiter waiter(*personal_data_);
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
    PersonalDataChangedWaiter(*personal_data_).Wait();
    iban.set_record_type(Iban::kLocalIban);
  }

  // Populates payments autofill table with credit card benefits data.
  void SetCreditCardBenefits(
      const std::vector<CreditCardBenefit>& credit_card_benefits) {
    GetServerDataTable()->SetCreditCardBenefits(credit_card_benefits);
  }

  std::unique_ptr<PersonalDataManager> personal_data_;
};

class PaymentsDataManagerTest : public PaymentsDataManagerHelper,
                                public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager();
  }
  void TearDown() override { TearDownTest(); }
};

class PaymentsDataManagerSyncTransportModeTest
    : public PaymentsDataManagerHelper,
      public testing::Test {
 protected:
  void SetUp() override {
    SetUpTest();
    ResetPersonalDataManager(
        /*use_sync_transport_mode=*/true);
  }
  void TearDown() override { TearDownTest(); }
};

// Test that server IBANs can be added and automatically loaded/cached.
TEST_F(PaymentsDataManagerTest, AddAndReloadServerIbans) {
  Iban server_iban1 = test::GetServerIban();
  Iban server_iban2 = test::GetServerIban2();

  GetServerDataTable()->SetServerIbansForTesting({server_iban1, server_iban2});
  std::vector<const Iban*> expected_ibans = {&server_iban1, &server_iban2};
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  ExpectSameElements(expected_ibans, personal_data_->GetServerIbans());

  // Reset the PersonalDataManager. This tests that the personal data was saved
  // to the web database, and that we can load the IBANs from the web database.
  ResetPersonalDataManager();

  // Verify that we've reloaded the IBANs from the web database.
  ExpectSameElements(expected_ibans, personal_data_->GetServerIbans());
}

// Test that all (local and server) IBANs can be returned.
TEST_F(PaymentsDataManagerTest, GetIbans) {
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
  PersonalDataChangedWaiter(*personal_data_).Wait();

  std::vector<const Iban*> all_ibans = {&local_iban1, &local_iban2,
                                        &server_iban1, &server_iban2};
  ExpectSameElements(all_ibans, personal_data_->GetIbans());
}

// Test that deduplication works correctly when a local IBAN has a matching
// prefix and suffix (either equal or starting with) and the same length as the
// server IBANs.
TEST_F(PaymentsDataManagerTest, GetIbansToSuggest) {
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
  PersonalDataChangedWaiter(*personal_data_).Wait();

  std::vector<const Iban*> ibans_to_suggest = {&server_iban1, &server_iban2,
                                               &local_iban2};
  ExpectSameElements(ibans_to_suggest, personal_data_->GetIbansToSuggest());
}

TEST_F(PaymentsDataManagerTest, AddLocalIbans) {
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
  // Do not add `PersonalDataChangedWaiter(*personal_data_).Wait()` for this
  // `AddAsLocalIban` operation, as it will be terminated prematurely for
  // `iban2_with_different_nickname` due to the presence of an IBAN with the
  // same value.
  personal_data_->AddAsLocalIban(iban2_with_different_nickname);

  std::vector<const Iban*> ibans = {&iban1, &iban2};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());
}

TEST_F(PaymentsDataManagerTest, AddingIbanUpdatesPref) {
  // The pref should always start disabled.
  ASSERT_FALSE(personal_data_->payments_data_manager()
                   .IsAutofillHasSeenIbanPrefEnabled());
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));

  personal_data_->AddAsLocalIban(iban);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  // Adding an IBAN permanently enables the pref.
  EXPECT_TRUE(personal_data_->payments_data_manager()
                  .IsAutofillHasSeenIbanPrefEnabled());
}

TEST_F(PaymentsDataManagerTest, UpdateLocalIbans) {
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
  PersonalDataChangedWaiter(*personal_data_).Wait();

  ibans = {&iban};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());

  // Update the `iban` with new nickname.
  iban.set_nickname(u"Another nickname");
  personal_data_->UpdateIban(iban);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  ibans = {&iban};
  ExpectSameElements(ibans, personal_data_->GetLocalIbans());
}

TEST_F(PaymentsDataManagerTest, RemoveLocalIbans) {
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

TEST_F(PaymentsDataManagerTest, RecordIbanUsage_LocalIban) {
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
  personal_data_->payments_data_manager().RecordUseOfIban(local_iban);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredIban.Local", 1);
  EXPECT_EQ(local_iban.use_count(), 2u);
  EXPECT_EQ(local_iban.use_date(), kSomeLaterTime);
  EXPECT_EQ(local_iban.modification_date(), kArbitraryTime);
}

TEST_F(PaymentsDataManagerTest, RecordIbanUsage_ServerIban) {
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
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Set the current time to sometime later.
  test_clock.SetNow(kSomeLaterTime);

  // Use `server_iban`, then verify usage stats.
  EXPECT_EQ(personal_data_->GetServerIbans().size(), 1u);
  personal_data_->payments_data_manager().RecordUseOfIban(server_iban);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredIban.Server", 1);
  EXPECT_EQ(server_iban.use_count(), 2u);
  EXPECT_EQ(server_iban.use_date(), kSomeLaterTime);
  EXPECT_EQ(server_iban.modification_date(), kArbitraryTime);
}

TEST_F(PaymentsDataManagerTest, AddUpdateRemoveCreditCards) {
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

  PersonalDataChangedWaiter(*personal_data_).Wait();

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

  PersonalDataChangedWaiter(*personal_data_).Wait();

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
  PersonalDataChangedWaiter(*personal_data_).Wait();

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

TEST_F(PaymentsDataManagerTest, RecordUseOfCard) {
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);
  CreditCard card = test::GetCreditCard();
  ASSERT_EQ(card.use_count(), 1u);
  ASSERT_EQ(card.use_date(), kArbitraryTime);
  ASSERT_EQ(card.modification_date(), kArbitraryTime);
  personal_data_->AddCreditCard(card);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  test_clock.SetNow(kSomeLaterTime);
  personal_data_->RecordUseOf(&card);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  CreditCard* pdm_card = personal_data_->GetCreditCardByGUID(card.guid());
  ASSERT_TRUE(pdm_card);
  EXPECT_EQ(pdm_card->use_count(), 2u);
  EXPECT_EQ(pdm_card->use_date(), kSomeLaterTime);
  EXPECT_EQ(pdm_card->modification_date(), kArbitraryTime);
}

// Test that UpdateLocalCvc function working as expected.
TEST_F(PaymentsDataManagerTest, UpdateLocalCvc) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  CreditCard credit_card = test::GetCreditCard();
  const std::u16string kCvc = u"111";
  credit_card.set_cvc(kCvc);
  PersonalDataChangedWaiter add_waiter(*personal_data_);
  personal_data_->AddCreditCard(credit_card);
  std::move(add_waiter).Wait();
  ASSERT_EQ(personal_data_->GetLocalCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetLocalCreditCards()[0]->cvc(), kCvc);

  const std::u16string kNewCvc = u"222";
  PersonalDataChangedWaiter update_waiter(*personal_data_);
  personal_data_->UpdateLocalCvc(credit_card.guid(), kNewCvc);
  std::move(update_waiter).Wait();
  ASSERT_EQ(personal_data_->GetLocalCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetLocalCreditCards()[0]->cvc(), kNewCvc);
}

// Test that verify add, update, remove server cvc function working as expected.
TEST_F(PaymentsDataManagerTest, ServerCvc) {
  const std::u16string kCvc = u"111";
  CreditCard credit_card = test::GetMaskedServerCard();
  SetServerCards({credit_card});

  // Add an empty cvc will fail a CHECK().
  EXPECT_DEATH_IF_SUPPORTED(personal_data_->AddServerCvc(1, u""), "");

  personal_data_->AddServerCvc(credit_card.instrument_id(), kCvc);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  ASSERT_EQ(personal_data_->GetCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetCreditCards()[0]->cvc(), kCvc);

  // Update an empty cvc will fail a CHECK().
  EXPECT_DEATH_IF_SUPPORTED(
      personal_data_->UpdateServerCvc(credit_card.instrument_id(), u""), "");
  // Update an non-exist card cvc will fail a CHECK().
  EXPECT_DEATH_IF_SUPPORTED(personal_data_->UpdateServerCvc(99999, u""), "");

  const std::u16string kNewCvc = u"222";
  personal_data_->UpdateServerCvc(credit_card.instrument_id(), kNewCvc);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_EQ(personal_data_->GetCreditCards()[0]->cvc(), kNewCvc);

  personal_data_->RemoveServerCvc(credit_card.instrument_id());
  PersonalDataChangedWaiter(*personal_data_).Wait();
  ASSERT_EQ(personal_data_->GetCreditCards().size(), 1U);
  EXPECT_TRUE(personal_data_->GetCreditCards()[0]->cvc().empty());
}

// Test that verify clear server cvc function working as expected.
TEST_F(PaymentsDataManagerTest, ClearServerCvc) {
  // Add a server card cvc.
  const std::u16string kCvc = u"111";
  CreditCard credit_card = test::GetMaskedServerCard();
  SetServerCards({credit_card});
  personal_data_->AddServerCvc(credit_card.instrument_id(), kCvc);
  PersonalDataChangedWaiter(*personal_data_).Wait();
  ASSERT_EQ(personal_data_->GetCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetCreditCards()[0]->cvc(), kCvc);

  // After we clear server cvcs we should expect empty cvc.
  personal_data_->ClearServerCvcs();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_TRUE(personal_data_->GetCreditCards()[0]->cvc().empty());
}

// Test that a new credit card has its basic information set.
TEST_F(PaymentsDataManagerTest, AddCreditCard_BasicInformation) {
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
TEST_F(PaymentsDataManagerTest, AddCreditCard_CrazyCharacters) {
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

  PersonalDataChangedWaiter(*personal_data_).Wait();

  ASSERT_EQ(cards.size(), personal_data_->GetCreditCards().size());
  for (size_t i = 0; i < cards.size(); ++i) {
    EXPECT_TRUE(base::Contains(cards, *personal_data_->GetCreditCards()[i]));
  }
}

// Test invalid credit card numbers typed in settings UI should be saved as-is.
TEST_F(PaymentsDataManagerTest, AddCreditCard_Invalid) {
  CreditCard card;
  card.SetRawInfo(CREDIT_CARD_NUMBER, u"Not_0123-5Checked");

  std::vector<CreditCard> cards;
  cards.push_back(card);
  personal_data_->SetCreditCards(&cards);

  ASSERT_EQ(1u, personal_data_->GetCreditCards().size());
  ASSERT_EQ(card, *personal_data_->GetCreditCards()[0]);
}

TEST_F(PaymentsDataManagerTest, GetCreditCardByServerId) {
  CreditCard card = test::GetFullServerCard();
  card.set_server_id("server id");
  personal_data_->AddFullServerCreditCardForTesting(card);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  ASSERT_EQ(1u, personal_data_->GetCreditCards().size());
  EXPECT_TRUE(personal_data_->GetCreditCardByServerId("server id"));
  EXPECT_FALSE(personal_data_->GetCreditCardByServerId("non-existing id"));
}

TEST_F(PaymentsDataManagerTest, UpdateUnverifiedCreditCards) {
  // Start with unverified data.
  CreditCard credit_card = test::GetCreditCard();
  EXPECT_FALSE(credit_card.IsVerified());

  // Add the data to the database.
  personal_data_->AddCreditCard(credit_card);
  PersonalDataChangedWaiter(*personal_data_).Wait();

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
  PersonalDataChangedWaiter(*personal_data_).Wait();

  EXPECT_THAT(personal_data_->GetCreditCards(),
              testing::UnorderedElementsAre(Pointee(credit_card)));
}

TEST_F(PaymentsDataManagerTest, SetUniqueCreditCardLabels) {
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

TEST_F(PaymentsDataManagerTest, SetEmptyCreditCard) {
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

// Tests that GetAutofillOffers returns all available offers.
TEST_F(PaymentsDataManagerTest, GetAutofillOffers) {
  // Add two card-linked offers and one promo code offer.
  AddOfferDataForTest(test::GetCardLinkedOfferData1());
  AddOfferDataForTest(test::GetCardLinkedOfferData2());
  AddOfferDataForTest(test::GetPromoCodeOfferData());

  // Should return all three.
  EXPECT_EQ(3U, personal_data_->GetAutofillOffers().size());
}

// Tests that GetActiveAutofillPromoCodeOffersForOrigin returns only active and
// site-relevant promo code offers.
TEST_F(PaymentsDataManagerTest, GetActiveAutofillPromoCodeOffersForOrigin) {
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

// Test that local credit cards are ordered as expected.
TEST_F(PaymentsDataManagerTest, GetCreditCardsToSuggest_LocalCardsRanking) {
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
TEST_F(PaymentsDataManagerTest,
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
  PersonalDataChangedWaiter(*personal_data_).Wait();
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

// Tests the suggestions of duplicate local and server credit cards.
TEST_F(PaymentsDataManagerTest, GetCreditCardsToSuggest_ServerDuplicates) {
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
  PersonalDataChangedWaiter(*personal_data_).Wait();
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
TEST_F(PaymentsDataManagerTest,
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
  PersonalDataChangedWaiter(*personal_data_).Wait();
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

  PersonalDataChangedWaiter(*personal_data_).Wait();

  card_to_suggest = personal_data_->GetCreditCardsToSuggest();
  ASSERT_EQ(3U, card_to_suggest.size());
}

// Tests that only the full server card is kept when deduping with a local
// duplicate of it.
TEST_F(PaymentsDataManagerTest,
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

  PaymentsDataManager::DedupeCreditCardToSuggest(&credit_cards);
  ASSERT_EQ(1U, credit_cards.size());

  const CreditCard* deduped_card = credit_cards.front();
  EXPECT_TRUE(*deduped_card == full_server_card);
}

// Tests that only the local card is kept when deduping with a masked server
// duplicate of it or vice-versa. This is checked based on the value assigned
// during the for loop.
TEST_F(PaymentsDataManagerTest,
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

    PaymentsDataManager::DedupeCreditCardToSuggest(&credit_cards);
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
TEST_F(PaymentsDataManagerTest, DedupeCreditCardToSuggest_FullServerAndMasked) {
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

  PaymentsDataManager::DedupeCreditCardToSuggest(&credit_cards);
  EXPECT_EQ(2U, credit_cards.size());
}

// Tests that different local, masked, and full server credit cards are not
// deduped.
TEST_F(PaymentsDataManagerTest, DedupeCreditCardToSuggest_DifferentCards) {
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

  PaymentsDataManager::DedupeCreditCardToSuggest(&credit_cards);
  EXPECT_EQ(3U, credit_cards.size());
}

TEST_F(PaymentsDataManagerTest, DeleteLocalCreditCards) {
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
  PersonalDataChangedWaiter(*personal_data_).Wait();

  EXPECT_EQ(1U, personal_data_->GetCreditCards().size());

  std::unordered_set<std::u16string> expectedToRemain = {u"Clyde"};
  for (auto* card : personal_data_->GetCreditCards()) {
    EXPECT_NE(expectedToRemain.end(),
              expectedToRemain.find(card->GetRawInfo(CREDIT_CARD_NAME_FULL)));
  }
}

TEST_F(PaymentsDataManagerTest, DeleteAllLocalCreditCards) {
  SetUpReferenceLocalCreditCards();

  // Expect 3 local credit cards.
  EXPECT_EQ(3U, personal_data_->GetLocalCreditCards().size());

  personal_data_->DeleteAllLocalCreditCards();

  // Wait for the data to be refreshed.
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Expect the local credit cards to have been deleted.
  EXPECT_EQ(0U, personal_data_->GetLocalCreditCards().size());
}

TEST_F(PaymentsDataManagerTest, LogStoredCreditCardMetrics) {
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
  PersonalDataChangedWaiter(*personal_data_).Wait();

  ASSERT_EQ(4U, personal_data_->GetCreditCards().size());

  // Reload the database, which will log the stored profile counts.
  base::HistogramTester histogram_tester;
  ResetPersonalDataManager();

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

// Sanity check that the mode where we use the regular, persistent storage for
// cards still works.
TEST_F(PaymentsDataManagerTest, UsePersistentServerStorage) {
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
TEST_F(PaymentsDataManagerSyncTransportModeTest, SwitchServerStorages) {
  // Start with account storage.
  SetUpTwoCardTypes();

  // Check that we do have a server card, as expected.
  ASSERT_EQ(1U, personal_data_->GetServerCreditCards().size());

  // Switch to persistent storage.
  sync_service_.SetHasSyncConsent(true);
  personal_data_->OnStateChanged(&sync_service_);
  PersonalDataChangedWaiter(*personal_data_).Wait();

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
  PersonalDataChangedWaiter(*personal_data_).Wait();

  EXPECT_EQ(1U, personal_data_->GetServerCreditCards().size());

  // Switch back to the account storage, and verify that we are back to the
  // original card.
  sync_service_.SetHasSyncConsent(false);
  personal_data_->OnStateChanged(&sync_service_);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  ASSERT_EQ(1U, personal_data_->GetServerCreditCards().size());
  EXPECT_EQ(u"3456", personal_data_->GetServerCreditCards()[0]->number());
}

// Sanity check that the mode where we use the regular, persistent storage for
// cards still works.
TEST_F(PaymentsDataManagerSyncTransportModeTest,
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

  PersonalDataChangedWaiter(*personal_data_).Wait();

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

  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Expect that the local card is stored in the profile autofill table.
  profile_autofill_table_->GetCreditCards(&cards);
  EXPECT_EQ(1U, cards.size());
  EXPECT_EQ(local_card.LastFourDigits(), cards[0]->LastFourDigits());
}

TEST_F(PaymentsDataManagerTest, ClearAllCvcs) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  // Add a server card and its CVC.
  CreditCard server_card = test::GetMaskedServerCard();
  const std::u16string server_cvc = u"111";
  SetServerCards({server_card});
  personal_data_->AddServerCvc(server_card.instrument_id(), server_cvc);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Add a local card and its CVC.
  CreditCard local_card = test::GetCreditCard();
  const std::u16string local_cvc = u"999";
  local_card.set_cvc(local_cvc);
  personal_data_->AddCreditCard(local_card);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  ASSERT_EQ(personal_data_->GetLocalCreditCards().size(), 1U);
  ASSERT_EQ(personal_data_->GetServerCreditCards().size(), 1U);
  EXPECT_EQ(personal_data_->GetServerCreditCards()[0]->cvc(), server_cvc);
  EXPECT_EQ(personal_data_->GetLocalCreditCards()[0]->cvc(), local_cvc);

  // Clear out all the CVCs (local + server).
  personal_data_->ClearLocalCvcs();
  personal_data_->ClearServerCvcs();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  EXPECT_TRUE(personal_data_->GetServerCreditCards()[0]->cvc().empty());
  EXPECT_TRUE(personal_data_->GetLocalCreditCards()[0]->cvc().empty());
}

// Tests that benefit getters return expected result for active benefits.
TEST_F(PaymentsDataManagerTest, GetActiveCreditCardBenefits) {
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
  EXPECT_TRUE(personal_data_->payments_data_manager()
                  .IsAutofillPaymentMethodsEnabled());
  EXPECT_EQ(
      personal_data_->payments_data_manager()
          .GetFlatRateBenefitByInstrumentId(instrument_id_for_flat_rate_benefit)
          ->linked_card_instrument_id(),
      instrument_id_for_flat_rate_benefit);

  std::optional<CreditCardCategoryBenefit> category_benefit_result =
      personal_data_->payments_data_manager()
          .GetCategoryBenefitByInstrumentIdAndCategory(
              instrument_id_for_category_benefit,
              benefit_category_for_category_benefit);
  EXPECT_EQ(category_benefit_result->linked_card_instrument_id(),
            instrument_id_for_category_benefit);
  EXPECT_EQ(category_benefit_result->benefit_category(),
            benefit_category_for_category_benefit);

  std::optional<CreditCardMerchantBenefit> merchant_benefit_result =
      personal_data_->payments_data_manager()
          .GetMerchantBenefitByInstrumentIdAndOrigin(
              instrument_id_for_merchant_benefit,
              merchant_origin_for_merchant_benefit);
  EXPECT_EQ(merchant_benefit_result->linked_card_instrument_id(),
            instrument_id_for_merchant_benefit);
  EXPECT_TRUE(merchant_benefit_result->merchant_domains().contains(
      merchant_origin_for_merchant_benefit));

  // Disable autofill credit card pref. Check that no benefits are returned.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  EXPECT_FALSE(
      personal_data_->payments_data_manager().GetFlatRateBenefitByInstrumentId(
          instrument_id_for_flat_rate_benefit));
  EXPECT_FALSE(personal_data_->payments_data_manager()
                   .GetCategoryBenefitByInstrumentIdAndCategory(
                       instrument_id_for_category_benefit,
                       benefit_category_for_category_benefit));
  EXPECT_FALSE(personal_data_->payments_data_manager()
                   .GetMerchantBenefitByInstrumentIdAndOrigin(
                       instrument_id_for_merchant_benefit,
                       merchant_origin_for_merchant_benefit));
}

// Tests benefit getters will not return inactive benefits.
TEST_F(PaymentsDataManagerTest, GetInactiveCreditCardBenefits) {
  // Add inactive benefits.
  base::Time future_time = AutofillClock::Now() + base::Days(5);

  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  test_api(flat_rate_benefit).SetStartTime(future_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_flat_rate_benefit =
          flat_rate_benefit.linked_card_instrument_id();
  personal_data_->AddCreditCardBenefitForTest(std::move(flat_rate_benefit));

  CreditCardCategoryBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  test_api(category_benefit).SetStartTime(future_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_category_benefit =
          category_benefit.linked_card_instrument_id();
  const CreditCardCategoryBenefit::BenefitCategory
      benefit_category_for_category_benefit =
          category_benefit.benefit_category();
  personal_data_->AddCreditCardBenefitForTest(std::move(category_benefit));

  CreditCardMerchantBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  test_api(merchant_benefit).SetStartTime(future_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_merchant_benefit =
          merchant_benefit.linked_card_instrument_id();
  url::Origin merchant_origin_for_merchant_benefit =
      *merchant_benefit.merchant_domains().begin();
  personal_data_->AddCreditCardBenefitForTest(std::move(merchant_benefit));

  // Should not return any benefits as no benefit is currently active.
  EXPECT_FALSE(
      personal_data_->payments_data_manager().GetFlatRateBenefitByInstrumentId(
          instrument_id_for_flat_rate_benefit));
  EXPECT_FALSE(personal_data_->payments_data_manager()
                   .GetCategoryBenefitByInstrumentIdAndCategory(
                       instrument_id_for_category_benefit,
                       benefit_category_for_category_benefit));
  EXPECT_FALSE(personal_data_->payments_data_manager()
                   .GetMerchantBenefitByInstrumentIdAndOrigin(
                       instrument_id_for_merchant_benefit,
                       merchant_origin_for_merchant_benefit));
}

// Tests benefit getters will not return expired benefits.
TEST_F(PaymentsDataManagerTest, GetExpiredCreditCardBenefits) {
  // Add Expired benefits.
  base::Time expired_time = AutofillClock::Now() - base::Days(5);

  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  test_api(flat_rate_benefit).SetExpiryTime(expired_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_flat_rate_benefit =
          flat_rate_benefit.linked_card_instrument_id();
  personal_data_->AddCreditCardBenefitForTest(std::move(flat_rate_benefit));

  CreditCardCategoryBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  test_api(category_benefit).SetExpiryTime(expired_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_category_benefit =
          category_benefit.linked_card_instrument_id();
  const CreditCardCategoryBenefit::BenefitCategory
      benefit_category_for_category_benefit =
          category_benefit.benefit_category();
  personal_data_->AddCreditCardBenefitForTest(std::move(category_benefit));

  CreditCardMerchantBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  test_api(merchant_benefit).SetExpiryTime(expired_time);
  const CreditCardBenefitBase::LinkedCardInstrumentId
      instrument_id_for_merchant_benefit =
          merchant_benefit.linked_card_instrument_id();
  url::Origin merchant_origin_for_merchant_benefit =
      *merchant_benefit.merchant_domains().begin();
  personal_data_->AddCreditCardBenefitForTest(std::move(merchant_benefit));

  // Should not return any benefits as all of the benefits are expired.
  EXPECT_FALSE(
      personal_data_->payments_data_manager().GetFlatRateBenefitByInstrumentId(
          instrument_id_for_flat_rate_benefit));
  EXPECT_FALSE(personal_data_->payments_data_manager()
                   .GetCategoryBenefitByInstrumentIdAndCategory(
                       instrument_id_for_category_benefit,
                       benefit_category_for_category_benefit));
  EXPECT_FALSE(personal_data_->payments_data_manager()
                   .GetMerchantBenefitByInstrumentIdAndOrigin(
                       instrument_id_for_merchant_benefit,
                       merchant_origin_for_merchant_benefit));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(PaymentsDataManagerTest, GetMaskedBankAccounts_ExpOff) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  BankAccount bank_account1 = test::CreatePixBankAccount(1234L);
  BankAccount bank_account2 = test::CreatePixBankAccount(5678L);
  ASSERT_TRUE(GetServerDataTable()->SetMaskedBankAccounts(
      {bank_account1, bank_account2}));
  std::vector<BankAccount> bank_accounts =
      personal_data_->GetMaskedBankAccounts();
  // Since the PersonalDataManager was initialized before adding the masked
  // bank accounts to the WebDatabase, we expect GetMaskedBankAccounts to return
  // an empty list.
  EXPECT_EQ(0u, bank_accounts.size());

  // Refresh the PersonalDataManager. Under normal circumstances with the flag
  // on, this step would load the bank accounts from the WebDatabase.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Verify that no bank accounts are loaded into PersonalDataManager because
  // the experiment is turned off.
  bank_accounts = personal_data_->GetMaskedBankAccounts();
  EXPECT_EQ(0u, bank_accounts.size());
}

TEST_F(PaymentsDataManagerTest, GetMaskedBankAccounts_PaymentMethodsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  BankAccount bank_account1 = test::CreatePixBankAccount(1234L);
  BankAccount bank_account2 = test::CreatePixBankAccount(5678L);
  ASSERT_TRUE(GetServerDataTable()->SetMaskedBankAccounts(
      {bank_account1, bank_account2}));
  // We need to call `Refresh()` to ensure that the BankAccounts are loaded
  // again from the WebDatabase.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  // Disable payment methods prefs.
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);

  // Verify that no bank accounts are loaded into PersonalDataManager because
  // the AutofillPaymentMethodsEnabled pref is set to false.
  EXPECT_THAT(personal_data_->GetMaskedBankAccounts(), testing::IsEmpty());
}

TEST_F(PaymentsDataManagerTest, GetMaskedBankAccounts_DatabaseUpdated) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAutofillEnableSyncingOfPixBankAccounts);
  BankAccount bank_account1 = test::CreatePixBankAccount(1234L);
  BankAccount bank_account2 = test::CreatePixBankAccount(5678L);
  ASSERT_TRUE(GetServerDataTable()->SetMaskedBankAccounts(
      {bank_account1, bank_account2}));

  // Since the PersonalDataManager was initialized before adding the masked
  // bank accounts to the WebDatabase, we expect GetMaskedBankAccounts to return
  // an empty list.
  std::vector<BankAccount> bank_accounts =
      personal_data_->GetMaskedBankAccounts();
  EXPECT_EQ(0u, bank_accounts.size());

  // We need to call `Refresh()` to ensure that the BankAccounts are loaded
  // again from the WebDatabase.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  bank_accounts = personal_data_->GetMaskedBankAccounts();
  EXPECT_EQ(2u, bank_accounts.size());
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(PaymentsDataManagerTest,
       OnAutofillPaymentsCardBenefitsPrefChange_PrefIsOn_DoesNotClearBenefits) {
  // Add the card benefits to the web database.
  std::vector<CreditCardBenefit> card_benefits;
  card_benefits.push_back(test::GetActiveCreditCardFlatRateBenefit());
  card_benefits.push_back(test::GetActiveCreditCardCategoryBenefit());
  card_benefits.push_back(test::GetActiveCreditCardMerchantBenefit());
  SetCreditCardBenefits(card_benefits);
  // Refresh to load the card benefits from the web database.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  ASSERT_EQ(card_benefits.size(),
            test_api(personal_data_->payments_data_manager())
                .GetCreditCardBenefitsCount());

  prefs::SetPaymentCardBenefits(prefs_.get(), true);

  ASSERT_EQ(card_benefits.size(),
            test_api(personal_data_->payments_data_manager())
                .GetCreditCardBenefitsCount());
}

TEST_F(PaymentsDataManagerTest,
       OnAutofillPaymentsCardBenefitsPrefChange_PrefIsOff_ClearsCardBenefits) {
  // Add the card benefits to the web database.
  std::vector<CreditCardBenefit> card_benefits;
  card_benefits.push_back(test::GetActiveCreditCardFlatRateBenefit());
  card_benefits.push_back(test::GetActiveCreditCardCategoryBenefit());
  card_benefits.push_back(test::GetActiveCreditCardMerchantBenefit());
  SetCreditCardBenefits(card_benefits);
  // Refresh to load the card benefits from the web database.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();

  ASSERT_EQ(card_benefits.size(),
            test_api(personal_data_->payments_data_manager())
                .GetCreditCardBenefitsCount());

  // Disable autofill payment card benefits pref and check that no benefits
  // are returned.
  prefs::SetPaymentCardBenefits(prefs_.get(), false);
  ASSERT_EQ(0U, test_api(personal_data_->payments_data_manager())
                    .GetCreditCardBenefitsCount());
}

// Tests that card benefits are not saved in PaymentsDataManager if the card
// benefits pref is disabled.
TEST_F(PaymentsDataManagerTest,
       OnAutofillPaymentsCardBenefits_PrefIsOff_BenefitsAreNotReturned) {
  prefs::SetPaymentCardBenefits(prefs_.get(), false);

  // Add the card benefits to the web database.
  std::vector<CreditCardBenefit> card_benefits;
  CreditCardFlatRateBenefit flat_rate_benefit =
      test::GetActiveCreditCardFlatRateBenefit();
  CreditCardCategoryBenefit category_benefit =
      test::GetActiveCreditCardCategoryBenefit();
  CreditCardMerchantBenefit merchant_benefit =
      test::GetActiveCreditCardMerchantBenefit();
  card_benefits.push_back(flat_rate_benefit);
  card_benefits.push_back(category_benefit);
  card_benefits.push_back(merchant_benefit);
  SetCreditCardBenefits(card_benefits);

  // Refresh to load the card benefits from the web database. Make sure no card
  // benefits are saved to PaymentsDataManager.
  personal_data_->Refresh();
  PersonalDataChangedWaiter(*personal_data_).Wait();
  ASSERT_EQ(0u, test_api(personal_data_->payments_data_manager())
                    .GetCreditCardBenefitsCount());

  // Ensure no card benefits are returned.
  EXPECT_EQ(
      std::nullopt,
      personal_data_->payments_data_manager().GetFlatRateBenefitByInstrumentId(
          flat_rate_benefit.linked_card_instrument_id()));
  EXPECT_EQ(std::nullopt,
            personal_data_->payments_data_manager()
                .GetMerchantBenefitByInstrumentIdAndOrigin(
                    merchant_benefit.linked_card_instrument_id(),
                    *merchant_benefit.merchant_domains().begin()));
  EXPECT_EQ(std::nullopt, personal_data_->payments_data_manager()
                              .GetCategoryBenefitByInstrumentIdAndCategory(
                                  category_benefit.linked_card_instrument_id(),
                                  category_benefit.benefit_category()));
}

#if !BUILDFLAG(IS_IOS)
TEST_F(PaymentsDataManagerTest, AddAndGetCreditCardArtImage) {
  gfx::Image expected_image = gfx::test::CreateImage(40, 24);
  std::unique_ptr<CreditCardArtImage> credit_card_art_image =
      std::make_unique<CreditCardArtImage>(GURL("https://www.example.com"),
                                           expected_image);
  std::vector<std::unique_ptr<CreditCardArtImage>> images;
  images.push_back(std::move(credit_card_art_image));
  test_api(personal_data_->payments_data_manager())
      .OnCardArtImagesFetched(std::move(images));

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

TEST_F(PaymentsDataManagerTest,
       TestNoImageFetchingAttemptForCardsWithInvalidCardArtUrls) {
  base::HistogramTester histogram_tester;

  gfx::Image* actual_image =
      personal_data_->GetCreditCardArtImageForUrl(GURL());
  EXPECT_FALSE(actual_image);
  EXPECT_EQ(0, histogram_tester.GetTotalSum("Autofill.ImageFetcher.Result"));
}

class MockAutofillImageFetcher : public AutofillImageFetcherBase {
 public:
  MOCK_METHOD(
      void,
      FetchImagesForURLs,
      (base::span<const GURL> card_art_urls,
       base::OnceCallback<void(
           const std::vector<std::unique_ptr<CreditCardArtImage>>&)> callback),
      (override));
};

TEST_F(PaymentsDataManagerTest, ProcessCardArtUrlChanges) {
  MockAutofillImageFetcher mock_image_fetcher;
  test_api(personal_data_->payments_data_manager())
      .SetImageFetcher(&mock_image_fetcher);
  auto wait_for_fetch_images_for_url = [&] {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_image_fetcher, FetchImagesForURLs)
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
    run_loop.Run();
  };

  CreditCard card = test::GetFullServerCard();
  card.set_server_id("card_server_id");
  personal_data_->AddFullServerCreditCardForTesting(card);
  PersonalDataChangedWaiter(*personal_data_).Wait();

  card.set_server_id("card_server_id");
  card.set_card_art_url(GURL("https://www.example.com/card1"));
  std::vector<GURL> updated_urls;
  updated_urls.emplace_back("https://www.example.com/card1");

  personal_data_->AddFullServerCreditCardForTesting(card);
  wait_for_fetch_images_for_url();

  card.set_card_art_url(GURL("https://www.example.com/card2"));
  updated_urls.clear();
  updated_urls.emplace_back("https://www.example.com/card2");

  personal_data_->AddFullServerCreditCardForTesting(card);
  wait_for_fetch_images_for_url();
}
#endif

// Params:
// 1. Whether the benefits toggle is turned on or off.
// 2. Whether the American Express benefits flag is enabled.
// 3. Whether the Capital One benefits flag is enabled.
class PaymentsDataManagerStartupBenefitsTest
    : public PaymentsDataManagerHelper,
      public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  PaymentsDataManagerStartupBenefitsTest() {
    feature_list_.InitWithFeatureStates(
        /*feature_states=*/
        {{features::kAutofillEnableCardBenefitsForAmericanExpress,
          AreAmericanExpressBenefitsEnabled()},
         {features::kAutofillEnableCardBenefitsForCapitalOne,
          AreCapitalOneBenefitsEnabled()}});
    SetUpTest();
  }

  ~PaymentsDataManagerStartupBenefitsTest() override = default;

  bool IsBenefitsPrefTurnedOn() const { return std::get<0>(GetParam()); }
  bool AreAmericanExpressBenefitsEnabled() const {
    return std::get<1>(GetParam());
  }
  bool AreCapitalOneBenefitsEnabled() const { return std::get<2>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         PaymentsDataManagerStartupBenefitsTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

// Tests that on startup we log the value of the card benefits pref.
TEST_P(PaymentsDataManagerStartupBenefitsTest,
       LogIsCreditCardBenefitsEnabledAtStartup) {
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), true);
  prefs::SetPaymentCardBenefits(prefs_.get(), IsBenefitsPrefTurnedOn());
  base::HistogramTester histogram_tester;
  ResetPersonalDataManager();
  if (!AreAmericanExpressBenefitsEnabled() && !AreCapitalOneBenefitsEnabled()) {
    histogram_tester.ExpectTotalCount(
        "Autofill.PaymentMethods.CardBenefitsIsEnabled.Startup", 0);
  } else {
    histogram_tester.ExpectUniqueSample(
        "Autofill.PaymentMethods.CardBenefitsIsEnabled.Startup",
        IsBenefitsPrefTurnedOn(), 1);
  }
}

// Tests that on startup if payment methods are disabled we don't log if
// benefits are enabled/disabled.
TEST_F(PaymentsDataManagerTest,
       LogIsCreditCardBenefitsEnabledAtStartup_PaymentMethodsDisabled) {
  prefs::SetAutofillPaymentMethodsEnabled(prefs_.get(), false);
  base::HistogramTester histogram_tester;
  ResetPersonalDataManager();
  histogram_tester.ExpectTotalCount(
      "Autofill.PaymentMethods.CardBenefitsIsEnabled.Startup", 0);
}

// Tests that on startup if there is no pref service for the PaymentsDataManager
// we don't log if benefits are enabled/disabled.
TEST_F(PaymentsDataManagerTest,
       LogIsCreditCardBenefitsEnabledAtStartup_NullPrefService) {
  base::HistogramTester histogram_tester;
  PaymentsDataManager payments_data_manager =
      PaymentsDataManager(/*profile_database=*/nullptr,
                          /*account_database=*/nullptr,
                          /*image_fetcher=*/nullptr,
                          /*shared_storage_handler=*/nullptr,
                          /*pref_service=*/nullptr,
                          /*app-locale=*/"en-US", personal_data_.get());

  histogram_tester.ExpectTotalCount(
      "Autofill.PaymentMethods.CardBenefitsIsEnabled.Startup", 0);
}

}  // namespace autofill
