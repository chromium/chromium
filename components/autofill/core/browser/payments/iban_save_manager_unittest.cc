// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_save_manager.h"

#include "base/guid.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/payments/iban_save_strike_database.h"
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
                         /*strike_database=*/nullptr,
                         /*image_fetcher=*/nullptr,
                         /*is_off_the_record=*/false);
    iban_save_manager_ = std::make_unique<IBANSaveManager>(&autofill_client_);
  }

  void OnUserDidDecideOnLocalSave(
      AutofillClient::SaveIBANOfferUserDecision user_decision,
      const absl::optional<std::u16string>& nickname = absl::nullopt) {
    iban_save_manager_->OnUserDidDecideOnLocalSaveForTesting(user_decision,
                                                             nickname);
  }

 protected:
  TestPersonalDataManager& personal_data() {
    return static_cast<TestPersonalDataManager&>(
        *autofill_client_.GetPersonalDataManager());
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillEnvironment autofill_environment_;
  TestAutofillClient autofill_client_{
      std::make_unique<TestPersonalDataManager>()};

  std::unique_ptr<IBANSaveManager> iban_save_manager_;
  raw_ptr<TestStrikeDatabase> strike_database_;
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(IBANSaveManagerTest, AttemptToOfferIBANLocalSave_validIBAN) {
  IBAN iban(base::GenerateGUID());
  iban.set_value(u"DE91 1000 0000 0123 4567 89");

  EXPECT_TRUE(iban_save_manager_->AttemptToOfferIBANLocalSave(iban));
}

TEST_F(IBANSaveManagerTest, AttemptToOfferIBANLocalSave_NoIBAN) {
  EXPECT_FALSE(iban_save_manager_->AttemptToOfferIBANLocalSave(absl::nullopt));
}

TEST_F(IBANSaveManagerTest, AttemptToOfferIBANLocalSave_IsOffTheRecord) {
  personal_data().set_is_off_the_record_for_testing(true);

  IBAN iban(base::GenerateGUID());
  iban.set_value(u"DE91 1000 0000 0123 4567 89");

  EXPECT_FALSE(iban_save_manager_->AttemptToOfferIBANLocalSave(iban));
}

TEST_F(IBANSaveManagerTest, OnUserDidDecideOnLocalSave_Accepted) {
  IBAN iban(base::GenerateGUID());
  std::u16string value = u"DE91 1000 0000 0123 4567 89";
  iban.set_value(value);

  EXPECT_TRUE(iban_save_manager_->AttemptToOfferIBANLocalSave(iban));

  OnUserDidDecideOnLocalSave(
      AutofillClient::SaveIBANOfferUserDecision::kAccepted,
      u"  My teacher's IBAN ");
  const std::vector<IBAN*> ibans = personal_data().GetIBANs();

  // Verify IBAN has been successfully updated with the new nickname on accept.
  EXPECT_EQ(ibans.size(), 1U);
  EXPECT_EQ(ibans[0]->nickname(), u"My teacher's IBAN");
  EXPECT_EQ(ibans[0]->value(), value);
}

TEST_F(IBANSaveManagerTest, OnUserDidDecideOnLocalSave_Declined) {
  IBAN iban(base::GenerateGUID());
  std::u16string value = u"DE91 1000 0000 0123 4567 89";
  iban.set_value(value);

  EXPECT_TRUE(iban_save_manager_->AttemptToOfferIBANLocalSave(iban));
  EXPECT_TRUE(personal_data().GetIBANs().empty());

  OnUserDidDecideOnLocalSave(
      AutofillClient::SaveIBANOfferUserDecision::kDeclined);
  const std::vector<IBAN*> ibans = personal_data().GetIBANs();

  EXPECT_TRUE(personal_data().GetIBANs().empty());
}

TEST_F(IBANSaveManagerTest, OnUserDidDecideOnLocalSave_Ignored) {
  IBAN iban(base::GenerateGUID());
  std::u16string value = u"DE91 1000 0000 0123 4567 89";
  iban.set_value(value);

  EXPECT_TRUE(iban_save_manager_->AttemptToOfferIBANLocalSave(iban));
  EXPECT_TRUE(personal_data().GetIBANs().empty());

  OnUserDidDecideOnLocalSave(
      AutofillClient::SaveIBANOfferUserDecision::kIgnored);
  const std::vector<IBAN*> ibans = personal_data().GetIBANs();

  EXPECT_TRUE(personal_data().GetIBANs().empty());
}

TEST_F(IBANSaveManagerTest, LocallySaveIBAN_NotEnoughStrikesShouldOfferToSave) {
  IBAN iban(base::GenerateGUID());
  const std::u16string iban_value = u"DE91 1000 0000 0123 4567 89";
  iban.set_value(iban_value);

  IBANSaveStrikeDatabase iban_save_strike_database =
      IBANSaveStrikeDatabase(strike_database_);

  iban_save_strike_database.AddStrike(base::UTF16ToUTF8(iban_value));

  // Verify `iban_value` has been successfully added to the strike database.
  EXPECT_EQ(
      1, iban_save_strike_database.GetStrikes(base::UTF16ToUTF8(iban_value)));
  EXPECT_TRUE(iban_save_manager_->AttemptToOfferIBANLocalSave(iban));
}

TEST_F(IBANSaveManagerTest, LocallySaveIBAN_MaxStrikesShouldNotOfferToSave) {
  IBAN iban(base::GenerateGUID());
  const std::u16string iban_value = u"DE91 1000 0000 0123 4567 89";
  iban.set_value(iban_value);

  IBANSaveStrikeDatabase iban_save_strike_database =
      IBANSaveStrikeDatabase(strike_database_);

  for (int i = 0; i < iban_save_strike_database.GetMaxStrikesLimit(); ++i) {
    iban_save_strike_database.AddStrike(base::UTF16ToUTF8(iban_value));
  }
  EXPECT_EQ(
      iban_save_strike_database.GetMaxStrikesLimit(),
      iban_save_strike_database.GetStrikes(base::UTF16ToUTF8(iban_value)));

  EXPECT_FALSE(iban_save_manager_->AttemptToOfferIBANLocalSave(iban));
}

TEST_F(IBANSaveManagerTest, OnUserDidDecideOnLocalSave_Accepted_ClearsStrikes) {
  IBAN iban(base::GenerateGUID());
  const std::u16string iban_value = u"DE91 1000 0000 0123 4567 89";
  iban.set_value(iban_value);
  iban_save_manager_->AttemptToOfferIBANLocalSave(iban);

  IBANSaveStrikeDatabase iban_save_strike_database =
      IBANSaveStrikeDatabase(strike_database_);

  iban_save_strike_database.AddStrike(base::UTF16ToUTF8(iban_value));

  // Verify `iban_value` has been successfully added to the strike database.
  EXPECT_EQ(
      1, iban_save_strike_database.GetStrikes(base::UTF16ToUTF8(iban_value)));
  OnUserDidDecideOnLocalSave(
      AutofillClient::SaveIBANOfferUserDecision::kAccepted,
      u"My teacher's IBAN");

  // Verify `iban_value` has been cleared in the strike database.
  EXPECT_EQ(
      0, iban_save_strike_database.GetStrikes(base::UTF16ToUTF8(iban_value)));
}

TEST_F(IBANSaveManagerTest, OnUserDidDecideOnLocalSave_Declined_AddsStrike) {
  IBAN iban(base::GenerateGUID());
  const std::u16string iban_value = u"DE91 1000 0000 0123 4567 89";
  iban.set_value(iban_value);
  iban_save_manager_->AttemptToOfferIBANLocalSave(iban);

  IBANSaveStrikeDatabase iban_save_strike_database =
      IBANSaveStrikeDatabase(strike_database_);

  // Verify `iban_value` has been successfully added to the strike database.
  EXPECT_EQ(
      0, iban_save_strike_database.GetStrikes(base::UTF16ToUTF8(iban_value)));

  OnUserDidDecideOnLocalSave(
      AutofillClient::SaveIBANOfferUserDecision::kDeclined,
      u"My teacher's IBAN");

  // Verify `iban_value` has been added to the strike database.
  EXPECT_EQ(
      1, iban_save_strike_database.GetStrikes(base::UTF16ToUTF8(iban_value)));
}

TEST_F(IBANSaveManagerTest, OnUserDidDecideOnLocalSave_Ignored_AddsStrike) {
  IBAN iban(base::GenerateGUID());
  const std::u16string iban_value = u"DE91 1000 0000 0123 4567 89";
  iban.set_value(iban_value);
  iban_save_manager_->AttemptToOfferIBANLocalSave(iban);

  IBANSaveStrikeDatabase iban_save_strike_database =
      IBANSaveStrikeDatabase(strike_database_);

  // Verify `iban_value` has been successfully added to the strike database.
  EXPECT_EQ(
      0, iban_save_strike_database.GetStrikes(base::UTF16ToUTF8(iban_value)));

  OnUserDidDecideOnLocalSave(
      AutofillClient::SaveIBANOfferUserDecision::kDeclined,
      u"My teacher's IBAN");

  // Verify `iban_value` has been added to the strike database.
  EXPECT_EQ(
      1, iban_save_strike_database.GetStrikes(base::UTF16ToUTF8(iban_value)));
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace autofill
