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

class IbanSaveManagerTest : public testing::Test {
 public:
  IbanSaveManagerTest() {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    autofill_client_.set_personal_data_manager(
        std::make_unique<TestPersonalDataManager>());
    std::unique_ptr<TestStrikeDatabase> test_strike_database =
        std::make_unique<TestStrikeDatabase>();
    strike_database_ = test_strike_database.get();
    autofill_client_.set_test_strike_database(std::move(test_strike_database));
    prefs::SetAutofillIbanEnabled(autofill_client_.GetPrefs(), true);
    personal_data().Init(/*profile_database=*/nullptr,
                         /*account_database=*/nullptr,
                         /*pref_service=*/autofill_client_.GetPrefs(),
                         /*local_state=*/autofill_client_.GetPrefs(),
                         /*identity_manager=*/nullptr,
                         /*history_service=*/nullptr,
                         /*sync_service=*/nullptr,
                         /*strike_database=*/nullptr,
                         /*image_fetcher=*/nullptr);
    iban_save_manager_ = std::make_unique<IbanSaveManager>(&autofill_client_);
  }

  IbanSaveManager& GetIbanSaveManager() { return *iban_save_manager_; }

 protected:
  TestPersonalDataManager& personal_data() {
    return static_cast<TestPersonalDataManager&>(
        *autofill_client_.GetPersonalDataManager());
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;

  std::unique_ptr<IbanSaveManager> iban_save_manager_;
  raw_ptr<TestStrikeDatabase> strike_database_;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(IbanSaveManagerTest, AttemptToOfferIbanLocalSave_ValidIban) {
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferIbanLocalSave(
      autofill::test::GetIban()));
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Accepted) {
  Iban iban = autofill::test::GetIban();
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferIbanLocalSave(iban));

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kAccepted,
      u"  My teacher's IBAN ");
  const std::vector<Iban*> ibans = personal_data().GetLocalIbans();

  // Verify IBAN has been successfully updated with the new nickname on accept.
  EXPECT_EQ(ibans.size(), 1U);
  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kAccepted,
      u"  My teacher's IBAN ");
  EXPECT_EQ(ibans[0]->value(), iban.value());
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Declined) {
  EXPECT_TRUE(iban_save_manager_->AttemptToOfferIbanLocalSave(
      autofill::test::GetIban()));
  EXPECT_TRUE(personal_data().GetLocalIbans().empty());

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kDeclined);
  const std::vector<Iban*> ibans = personal_data().GetLocalIbans();

  EXPECT_TRUE(personal_data().GetLocalIbans().empty());
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Ignored) {
  EXPECT_TRUE(iban_save_manager_->AttemptToOfferIbanLocalSave(
      autofill::test::GetIban()));
  EXPECT_TRUE(personal_data().GetLocalIbans().empty());

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kIgnored);
  const std::vector<Iban*> ibans = personal_data().GetLocalIbans();

  EXPECT_TRUE(personal_data().GetLocalIbans().empty());
}

TEST_F(IbanSaveManagerTest, LocallySaveIban_NotEnoughStrikesShouldOfferToSave) {
  Iban iban = autofill::test::GetIban();
  const std::string partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(partial_iban_hash);

  // Verify `kIbanValue` has been successfully added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferIbanLocalSave(iban));
}

TEST_F(IbanSaveManagerTest, LocallySaveIban_MaxStrikesShouldNotOfferToSave) {
  Iban iban = autofill::test::GetIban();
  const std::string partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));
  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrikes(
      iban_save_strike_database.GetMaxStrikesLimit(), partial_iban_hash);

  EXPECT_EQ(iban_save_strike_database.GetMaxStrikesLimit(),
            iban_save_strike_database.GetStrikes(partial_iban_hash));
  EXPECT_FALSE(GetIbanSaveManager().AttemptToOfferIbanLocalSave(iban));
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Accepted_ClearsStrikes) {
  Iban iban = autofill::test::GetIban();
  const std::string partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferIbanLocalSave(iban));

  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(partial_iban_hash);

  // Verify partial hashed value of `partial_iban_hash` has been
  // successfully added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kAccepted,
      u"My teacher's IBAN");

  // Verify partial hashed value of `partial_iban_hash` has been
  // cleared in the strike database.
  EXPECT_EQ(0, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Declined_AddsStrike) {
  Iban iban = autofill::test::GetIban();
  const std::string partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferIbanLocalSave(iban));

  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);

  EXPECT_EQ(0, iban_save_strike_database.GetStrikes(partial_iban_hash));

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kDeclined,
      u"My teacher's IBAN");

  // Verify partial hashed value of `partial_iban_hash` has been
  // added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Ignored_AddsStrike) {
  Iban iban = autofill::test::GetIban();
  const std::string partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferIbanLocalSave(iban));

  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);

  EXPECT_EQ(0, iban_save_strike_database.GetStrikes(partial_iban_hash));

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kDeclined,
      u"My teacher's IBAN");

  // Verify partial hashed value of `partial_iban_hash` has been
  // added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IbanSaveManagerTest, LocallySaveIban_AttemptToOfferIbanLocalSave) {
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferIbanLocalSave(
      autofill::test::GetIban()));
  EXPECT_TRUE(autofill_client_.ConfirmSaveIbanLocallyWasCalled());
}

TEST_F(IbanSaveManagerTest,
       LocallySaveIban_MaxStrikesShouldNotOfferToSave_Metrics) {
  base::HistogramTester histogram_tester;
  Iban iban(base::Uuid::GenerateRandomV4().AsLowercaseString());
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrikes(
      iban_save_strike_database.GetMaxStrikesLimit(),
      IbanSaveManager::GetPartialIbanHashString(
          test::GetStrippedValue(test::kIbanValue)));

  EXPECT_EQ(iban_save_strike_database.GetMaxStrikesLimit(),
            iban_save_strike_database.GetStrikes(
                IbanSaveManager::GetPartialIbanHashString(
                    test::GetStrippedValue(test::kIbanValue))));
  EXPECT_FALSE(GetIbanSaveManager().AttemptToOfferIbanLocalSave(iban));
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.IbanSaveNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::LOCAL, 1);
}

TEST_F(IbanSaveManagerTest, StrikesPresentWhenIbanSaved_Local) {
  base::HistogramTester histogram_tester;
  Iban iban(base::Uuid::GenerateRandomV4().AsLowercaseString());
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(IbanSaveManager::GetPartialIbanHashString(
      test::GetStrippedValue(test::kIbanValue)));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferIbanLocalSave(
      autofill::test::GetIban()));
  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kAccepted,
      u"My teacher's IBAN");

  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.StrikesPresentWhenIbanSaved.Local",
      /*sample=*/1, /*expected_count=*/1);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace autofill
