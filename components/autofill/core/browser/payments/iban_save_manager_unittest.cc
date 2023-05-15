// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_save_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class IBANSaveManagerTest : public testing::Test {
 public:
  IBANSaveManagerTest() {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    autofill_client_.set_personal_data_manager(
        std::make_unique<TestPersonalDataManager>());
    std::unique_ptr<TestStrikeDatabase> test_strike_database =
        std::make_unique<TestStrikeDatabase>();
    strike_database_ = test_strike_database.get();
    autofill_client_.set_test_strike_database(std::move(test_strike_database));
    prefs::SetAutofillIBANEnabled(autofill_client_.GetPrefs(), true);
    personal_data().Init(/*profile_database=*/nullptr,
                         /*account_database=*/nullptr,
                         /*pref_service=*/autofill_client_.GetPrefs(),
                         /*local_state=*/autofill_client_.GetPrefs(),
                         /*identity_manager=*/nullptr,
                         /*history_service=*/nullptr,
                         /*sync_service=*/nullptr,
                         /*strike_database=*/nullptr,
                         /*image_fetcher=*/nullptr,
                         /*is_off_the_record=*/false);
    iban_save_manager_ = std::make_unique<IBANSaveManager>(&autofill_client_);
  }

  IBANSaveManager& GetIBANSaveManager() { return *iban_save_manager_; }

 protected:
  TestPersonalDataManager& personal_data() {
    return static_cast<TestPersonalDataManager&>(
        *autofill_client_.GetPersonalDataManager());
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;

  std::unique_ptr<IBANSaveManager> iban_save_manager_;
  raw_ptr<TestStrikeDatabase> strike_database_;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(IBANSaveManagerTest, AttemptToOfferIBANLocalSave_ValidIBAN) {
  EXPECT_TRUE(GetIBANSaveManager().AttemptToOfferIBANLocalSave(
      autofill::test::GetIBAN()));
}

TEST_F(IBANSaveManagerTest, AttemptToOfferIBANLocalSave_IsOffTheRecord) {
  personal_data().set_is_off_the_record_for_testing(true);

  EXPECT_FALSE(GetIBANSaveManager().AttemptToOfferIBANLocalSave(
      autofill::test::GetIBAN()));
}

TEST_F(IBANSaveManagerTest, OnUserDidDecideOnLocalSave_Accepted) {
  IBAN iban = autofill::test::GetIBAN();
  EXPECT_TRUE(GetIBANSaveManager().AttemptToOfferIBANLocalSave(iban));

  GetIBANSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIBANOfferUserDecision::kAccepted,
      u"  My teacher's IBAN ");
  const std::vector<IBAN*> ibans = personal_data().GetLocalIBANs();

  // Verify IBAN has been successfully updated with the new nickname on accept.
  EXPECT_EQ(ibans.size(), 1U);
  GetIBANSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIBANOfferUserDecision::kAccepted,
      u"  My teacher's IBAN ");
  EXPECT_EQ(ibans[0]->value(), iban.value());
}

TEST_F(IBANSaveManagerTest, OnUserDidDecideOnLocalSave_Declined) {
  EXPECT_TRUE(iban_save_manager_->AttemptToOfferIBANLocalSave(
      autofill::test::GetIBAN()));
  EXPECT_TRUE(personal_data().GetLocalIBANs().empty());

  GetIBANSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIBANOfferUserDecision::kDeclined);
  const std::vector<IBAN*> ibans = personal_data().GetLocalIBANs();

  EXPECT_TRUE(personal_data().GetLocalIBANs().empty());
}

TEST_F(IBANSaveManagerTest, OnUserDidDecideOnLocalSave_Ignored) {
  EXPECT_TRUE(iban_save_manager_->AttemptToOfferIBANLocalSave(
      autofill::test::GetIBAN()));
  EXPECT_TRUE(personal_data().GetLocalIBANs().empty());

  GetIBANSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIBANOfferUserDecision::kIgnored);
  const std::vector<IBAN*> ibans = personal_data().GetLocalIBANs();

  EXPECT_TRUE(personal_data().GetLocalIBANs().empty());
}

TEST_F(IBANSaveManagerTest, LocallySaveIBAN_NotEnoughStrikesShouldOfferToSave) {
  IBAN iban = autofill::test::GetIBAN();
  const std::string partial_iban_hash =
      IBANSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  IBANSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(partial_iban_hash);

  // Verify `kIbanValue` has been successfully added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
  EXPECT_TRUE(GetIBANSaveManager().AttemptToOfferIBANLocalSave(iban));
}

TEST_F(IBANSaveManagerTest, LocallySaveIBAN_MaxStrikesShouldNotOfferToSave) {
  IBAN iban = autofill::test::GetIBAN();
  const std::string partial_iban_hash =
      IBANSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));
  IBANSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrikes(
      iban_save_strike_database.GetMaxStrikesLimit(), partial_iban_hash);

  EXPECT_EQ(iban_save_strike_database.GetMaxStrikesLimit(),
            iban_save_strike_database.GetStrikes(partial_iban_hash));
  EXPECT_FALSE(GetIBANSaveManager().AttemptToOfferIBANLocalSave(iban));
}

TEST_F(IBANSaveManagerTest, OnUserDidDecideOnLocalSave_Accepted_ClearsStrikes) {
  IBAN iban = autofill::test::GetIBAN();
  const std::string partial_iban_hash =
      IBANSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  EXPECT_TRUE(GetIBANSaveManager().AttemptToOfferIBANLocalSave(iban));

  IBANSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(partial_iban_hash);

  // Verify partial hashed value of `partial_iban_hash` has been
  // successfully added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
  GetIBANSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIBANOfferUserDecision::kAccepted,
      u"My teacher's IBAN");

  // Verify partial hashed value of `partial_iban_hash` has been
  // cleared in the strike database.
  EXPECT_EQ(0, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IBANSaveManagerTest, OnUserDidDecideOnLocalSave_Declined_AddsStrike) {
  IBAN iban = autofill::test::GetIBAN();
  const std::string partial_iban_hash =
      IBANSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  EXPECT_TRUE(GetIBANSaveManager().AttemptToOfferIBANLocalSave(iban));

  IBANSaveStrikeDatabase iban_save_strike_database(strike_database_);

  EXPECT_EQ(0, iban_save_strike_database.GetStrikes(partial_iban_hash));

  GetIBANSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIBANOfferUserDecision::kDeclined,
      u"My teacher's IBAN");

  // Verify partial hashed value of `partial_iban_hash` has been
  // added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IBANSaveManagerTest, OnUserDidDecideOnLocalSave_Ignored_AddsStrike) {
  IBAN iban = autofill::test::GetIBAN();
  const std::string partial_iban_hash =
      IBANSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  EXPECT_TRUE(GetIBANSaveManager().AttemptToOfferIBANLocalSave(iban));

  IBANSaveStrikeDatabase iban_save_strike_database(strike_database_);

  EXPECT_EQ(0, iban_save_strike_database.GetStrikes(partial_iban_hash));

  GetIBANSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIBANOfferUserDecision::kDeclined,
      u"My teacher's IBAN");

  // Verify partial hashed value of `partial_iban_hash` has been
  // added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IBANSaveManagerTest, LocallySaveIBAN_AttemptToOfferIBANLocalSave) {
  EXPECT_TRUE(GetIBANSaveManager().AttemptToOfferIBANLocalSave(
      autofill::test::GetIBAN()));
  EXPECT_TRUE(autofill_client_.ConfirmSaveIBANLocallyWasCalled());
}

TEST_F(IBANSaveManagerTest,
       LocallySaveIBAN_MaxStrikesShouldNotOfferToSave_Metrics) {
  base::HistogramTester histogram_tester;
  IBAN iban(base::Uuid::GenerateRandomV4().AsLowercaseString());
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  IBANSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrikes(
      iban_save_strike_database.GetMaxStrikesLimit(),
      IBANSaveManager::GetPartialIbanHashString(
          test::GetStrippedValue(test::kIbanValue)));

  EXPECT_EQ(iban_save_strike_database.GetMaxStrikesLimit(),
            iban_save_strike_database.GetStrikes(
                IBANSaveManager::GetPartialIbanHashString(
                    test::GetStrippedValue(test::kIbanValue))));
  EXPECT_FALSE(GetIBANSaveManager().AttemptToOfferIBANLocalSave(iban));
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.IbanSaveNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::LOCAL, 1);
}

TEST_F(IBANSaveManagerTest, StrikesPresentWhenIBANSaved_Local) {
  base::HistogramTester histogram_tester;
  IBAN iban(base::Uuid::GenerateRandomV4().AsLowercaseString());
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  IBANSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(IBANSaveManager::GetPartialIbanHashString(
      test::GetStrippedValue(test::kIbanValue)));

  EXPECT_TRUE(GetIBANSaveManager().AttemptToOfferIBANLocalSave(
      autofill::test::GetIBAN()));
  GetIBANSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIBANOfferUserDecision::kAccepted,
      u"My teacher's IBAN");

  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.StrikesPresentWhenIbanSaved.Local",
      /*sample=*/1, /*expected_count=*/1);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace autofill
