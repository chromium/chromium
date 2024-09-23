// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_data_cleaner.h"

#include "base/i18n/time_formatting.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/payments_data_manager_test_base.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace autofill {

class PaymentsDataCleanerTest : public PaymentsDataManagerTestBase,
                                public testing::Test {
 public:
  PaymentsDataCleanerTest() = default;
  ~PaymentsDataCleanerTest() override = default;

  void SetUp() override {
    SetUpTest();
    MakePrimaryAccountAvailable(/*use_sync_transport_mode=*/false,
                                identity_test_env_, sync_service_);
    personal_data_ = std::make_unique<PersonalDataManager>(
        profile_database_service_, account_database_service_, prefs_.get(),
        prefs_.get(), identity_test_env_.identity_manager(),
        /*history_service=*/nullptr, &sync_service_,
        /*strike_database=*/nullptr,
        /*image_fetcher=*/nullptr, /*shared_storage_handler=*/nullptr, "en-US",
        "US");
    PersonalDataChangedWaiter(*personal_data_).Wait();
    payments_data_cleaner_ = std::make_unique<PaymentsDataCleaner>(
        &personal_data_->payments_data_manager());
  }

  void TearDown() override {
    payments_data_cleaner_.reset();
    if (personal_data_) {
      personal_data_->Shutdown();
    }
    personal_data_.reset();
    TearDownTest();
  }

 protected:
  void SetServerCards(const std::vector<CreditCard>& server_cards) {
    test::SetServerCreditCards(
        personal_data_->payments_data_manager()
                .IsSyncFeatureEnabledForPaymentsServerMetrics()
            ? profile_autofill_table_.get()
            : account_autofill_table_.get(),
        server_cards);
  }

  bool DeleteDisusedCreditCards() {
    return payments_data_cleaner_->DeleteDisusedCreditCards();
  }

  void ClearCreditCardNonSettingsOrigins() {
    payments_data_cleaner_->ClearCreditCardNonSettingsOrigins();
  }

  PersonalDataManager& personal_data() { return *personal_data_.get(); }

 private:
  std::unique_ptr<PersonalDataManager> personal_data_;
  std::unique_ptr<PaymentsDataCleaner> payments_data_cleaner_;
};

// Tests that DeleteDisusedCreditCards deletes desired credit cards only.
TEST_F(PaymentsDataCleanerTest,
       DeleteDisusedCreditCards_OnlyDeleteExpiredDisusedLocalCards) {
  // Move the time to 20XX.
  task_environment_.FastForwardBy(base::Days(365) * 31);
  const char kHistogramName[] = "Autofill.CreditCardsDeletedForDisuse";
  auto now = base::Time::Now();

  // Create a recently used local card, it is expected to remain.
  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Alice",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  credit_card1.set_use_date(now - base::Days(4));

  // Create a local card that was expired 400 days ago, but recently used.
  // It is expected to remain.
  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card2, "Bob",
                          "378282246310006" /* American Express */, "04",
                          "1999", "1");
  credit_card2.set_use_date(now - base::Days(4));

  // Create a local card expired recently, and last used 400 days ago.
  // It is expected to remain.
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  const base::Time expiry_date = now - base::Days(32);
  const std::string month = base::UnlocalizedTimeFormatWithPattern(
      expiry_date, "MM", icu::TimeZone::getGMT());
  const std::string year = base::UnlocalizedTimeFormatWithPattern(
      expiry_date, "yyyy", icu::TimeZone::getGMT());
  test::SetCreditCardInfo(&credit_card3, "Clyde", "4111111111111111" /* Visa */,
                          month.c_str(), year.c_str(), "1");
  credit_card3.set_use_date(now - base::Days(400));

  // Create a local card expired 400 days ago, and last used 400 days ago.
  // It is expected to be deleted.
  CreditCard credit_card4(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card4, "David",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  credit_card4.set_use_date(now - base::Days(400));
  personal_data().payments_data_manager().AddCreditCard(credit_card1);
  personal_data().payments_data_manager().AddCreditCard(credit_card2);
  personal_data().payments_data_manager().AddCreditCard(credit_card3);
  personal_data().payments_data_manager().AddCreditCard(credit_card4);

  // Create masked server card expired 400 days ago, and last used 400 days ago.
  // It is expected to remain because we do not delete server cards.
  CreditCard credit_card5(CreditCard::RecordType::kMaskedServerCard, "c987");
  test::SetCreditCardInfo(&credit_card5, "Frank", "6543", "01", "1998", "1");
  credit_card5.set_use_date(now - base::Days(400));
  credit_card5.SetNetworkForMaskedCard(kVisaCard);

  // Save the server card and set used_date to desired date.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(credit_card5);
  SetServerCards(server_cards);
  personal_data().payments_data_manager().UpdateServerCardsMetadata(
      {credit_card5});

  PersonalDataChangedWaiter(personal_data()).Wait();
  EXPECT_EQ(5U,
            personal_data().payments_data_manager().GetCreditCards().size());

  // Setup histograms capturing.
  base::HistogramTester histogram_tester;

  // DeleteDisusedCreditCards should return true to indicate it was run.
  EXPECT_TRUE(DeleteDisusedCreditCards());

  // Wait for the data to be refreshed.
  PersonalDataChangedWaiter(personal_data()).Wait();

  EXPECT_EQ(4U,
            personal_data().payments_data_manager().GetCreditCards().size());
  std::unordered_set<std::u16string> expectedToRemain = {u"Alice", u"Bob",
                                                         u"Clyde", u"Frank"};
  for (auto* card : personal_data().payments_data_manager().GetCreditCards()) {
    EXPECT_NE(expectedToRemain.end(),
              expectedToRemain.find(card->GetRawInfo(CREDIT_CARD_NAME_FULL)));
  }

  // Verify histograms are logged.
  histogram_tester.ExpectTotalCount(kHistogramName, 1);
  histogram_tester.ExpectBucketCount(kHistogramName, 1, 1);
}

// Tests that all the non settings origins of autofill credit cards are cleared
// but that the settings origins are untouched.
TEST_F(PaymentsDataCleanerTest, ClearCreditCardNonSettingsOrigins) {
  // Create three cards with a non settings origin.
  CreditCard credit_card0(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "https://www.example.com");
  test::SetCreditCardInfo(&credit_card0, "Bob0",
                          "5105105105105100" /* Mastercard */, "04", "1999",
                          "1");
  credit_card0.set_use_count(10000);
  personal_data().payments_data_manager().AddCreditCard(credit_card0);

  CreditCard credit_card1(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          test::kEmptyOrigin);
  test::SetCreditCardInfo(&credit_card1, "Bob1",
                          "5105105105105101" /* Mastercard */, "04", "1999",
                          "1");
  credit_card1.set_use_count(1000);
  personal_data().payments_data_manager().AddCreditCard(credit_card1);

  CreditCard credit_card2(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          "1234");
  test::SetCreditCardInfo(&credit_card2, "Bob2",
                          "5105105105105102" /* Mastercard */, "04", "1999",
                          "1");
  credit_card2.set_use_count(100);
  personal_data().payments_data_manager().AddCreditCard(credit_card2);

  // Create a card with a settings origin.
  CreditCard credit_card3(base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card3, "Bob3",
                          "5105105105105103" /* Mastercard */, "04", "1999",
                          "1");
  credit_card3.set_use_count(10);
  personal_data().payments_data_manager().AddCreditCard(credit_card3);

  PersonalDataChangedWaiter(personal_data()).Wait();
  ASSERT_EQ(4U,
            personal_data().payments_data_manager().GetCreditCards().size());

  ClearCreditCardNonSettingsOrigins();

  PersonalDataChangedWaiter(personal_data()).Wait();
  ASSERT_EQ(4U,
            personal_data().payments_data_manager().GetCreditCards().size());

  // The first three profiles' origin should be cleared and the fourth one still
  // be the settings origin.
  EXPECT_TRUE(personal_data()
                  .payments_data_manager()
                  .GetCreditCardsToSuggest()[0]
                  ->origin()
                  .empty());
  EXPECT_TRUE(personal_data()
                  .payments_data_manager()
                  .GetCreditCardsToSuggest()[1]
                  ->origin()
                  .empty());
  EXPECT_TRUE(personal_data()
                  .payments_data_manager()
                  .GetCreditCardsToSuggest()[2]
                  ->origin()
                  .empty());
  EXPECT_EQ(kSettingsOrigin, personal_data()
                                 .payments_data_manager()
                                 .GetCreditCardsToSuggest()[3]
                                 ->origin());
}

}  // namespace autofill
