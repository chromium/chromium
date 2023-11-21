// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_save_manager.h"

#include "base/json/json_reader.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/payments/mock_test_payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
namespace {

constexpr char kLegalMessageLines[] =
    "{"
    "  \"line\" : [ {"
    "     \"template\": \"The legal documents are: {0} and {1}.\","
    "     \"template_parameter\" : [ {"
    "        \"display_text\" : \"Terms of Service\","
    "        \"url\": \"http://www.example.com/tos\""
    "     }, {"
    "        \"display_text\" : \"Privacy Policy\","
    "        \"url\": \"http://www.example.com/pp\""
    "     } ]"
    "  } ]"
    "}";

constexpr char kInvalidLegalMessageLines[] =
    "{"
    "  \"line\" : [ {"
    "     \"template\": \"Panda {0}.\","
    "     \"template_parameter\": [ {"
    "        \"display_text\": \"bear\""
    "     } ]"
    "  } ]"
    "}";

}  // namespace

class IbanSaveManagerTest : public testing::Test {
 public:
  IbanSaveManagerTest() {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    autofill_client_.set_personal_data_manager(
        std::make_unique<TestPersonalDataManager>());
    autofill_client_.set_test_payments_network_interface(
        std::make_unique<MockTestPaymentsNetworkInterface>());
    autofill_client_.set_sync_service(&sync_service_);
    std::unique_ptr<TestStrikeDatabase> test_strike_database =
        std::make_unique<TestStrikeDatabase>();
    strike_database_ = test_strike_database.get();
    autofill_client_.set_test_strike_database(std::move(test_strike_database));
    personal_data().SetSyncingForTest(true);
    personal_data().Init(/*profile_database=*/nullptr,
                         /*account_database=*/nullptr,
                         /*pref_service=*/autofill_client_.GetPrefs(),
                         /*local_state=*/autofill_client_.GetPrefs(),
                         /*identity_manager=*/nullptr,
                         /*history_service=*/nullptr,
                         /*sync_service=*/nullptr,
                         /*strike_database=*/nullptr,
                         /*image_fetcher=*/nullptr);
    iban_save_manager_ =
        std::make_unique<IbanSaveManager>(&personal_data(), &autofill_client_);
  }

  IbanSaveManager& GetIbanSaveManager() { return *iban_save_manager_; }

  void SetUpGetIbanUploadDetailsResponse(
      bool is_successful,
      bool includes_invalid_legal_message = false) {
    ON_CALL(*payments_network_interface(), GetIbanUploadDetails)
        .WillByDefault(
            [is_successful, includes_invalid_legal_message](
                const std::string& app_locale, int64_t billing_customer_number,
                int billable_service_number,
                base::OnceCallback<void(
                    AutofillClient::PaymentsRpcResult, const std::u16string&,
                    std::unique_ptr<base::Value::Dict>)> callback) {
              std::move(callback).Run(
                  is_successful
                      ? AutofillClient::PaymentsRpcResult::kSuccess
                      : AutofillClient::PaymentsRpcResult::kPermanentFailure,
                  u"this is a context token",
                  includes_invalid_legal_message
                      ? std::make_unique<base::Value::Dict>(
                            base::JSONReader::ReadDict(
                                kInvalidLegalMessageLines)
                                .value())
                      : std::make_unique<base::Value::Dict>(
                            base::JSONReader::ReadDict(kLegalMessageLines)
                                .value()));
            });
  }

  void SetUpUploadIbanResponse(bool is_successful) {
    ON_CALL(*payments_network_interface(), UploadIban)
        .WillByDefault(
            [is_successful](
                const payments::PaymentsNetworkInterface::
                    UploadIbanRequestDetails& request_details,
                base::OnceCallback<void(AutofillClient::PaymentsRpcResult)>
                    callback) {
              std::move(callback).Run(
                  is_successful
                      ? AutofillClient::PaymentsRpcResult::kSuccess
                      : AutofillClient::PaymentsRpcResult::kPermanentFailure);
            });
  }

 protected:
  TestPersonalDataManager& personal_data() {
    return static_cast<TestPersonalDataManager&>(
        *autofill_client_.GetPersonalDataManager());
  }

  MockTestPaymentsNetworkInterface* payments_network_interface() {
    return static_cast<MockTestPaymentsNetworkInterface*>(
        autofill_client_.GetPaymentsNetworkInterface());
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  syncer::TestSyncService sync_service_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<IbanSaveManager> iban_save_manager_;
  raw_ptr<TestStrikeDatabase> strike_database_;
  base::test::ScopedFeatureList feature_list_{
      features::kAutofillEnableServerIban};
};

TEST_F(IbanSaveManagerTest, AttemptToOfferSave_NewIban_ShouldOfferSave) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));
}

TEST_F(IbanSaveManagerTest, AttemptToOfferSave_LocalIban_ShouldOfferSave) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  personal_data().AddAsLocalIban(iban);

  Iban another_iban;
  another_iban.set_value(iban.value());
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));
}

// Test that new IBANs should not be offered upload save to Google Payments if
// flag is off.
TEST_F(IbanSaveManagerTest, ShouldOfferUploadSave_NewIban_FlagOff) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(features::kAutofillEnableServerIban);
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  EXPECT_EQ(IbanSaveManager::TypeOfOfferToSave::kOfferLocalSave,
            GetIbanSaveManager().DetermineHowToSaveIbanForTesting(iban));
}

// Test that new IBANs should be offered upload save to Google Payments.
TEST_F(IbanSaveManagerTest, ShouldOfferUploadSave_NewIban) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  EXPECT_EQ(IbanSaveManager::TypeOfOfferToSave::kOfferServerSave,
            GetIbanSaveManager().DetermineHowToSaveIbanForTesting(iban));
}

// Test that an existing local IBAN should still be offered upload save to
// Google Payments.
TEST_F(IbanSaveManagerTest, ShouldOfferUploadSave_LocalIban) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  personal_data().AddAsLocalIban(iban);

  Iban another_iban;
  another_iban.set_value(iban.value());
  EXPECT_EQ(IbanSaveManager::TypeOfOfferToSave::kOfferServerSave,
            GetIbanSaveManager().DetermineHowToSaveIbanForTesting(iban));
}

// Test that an existing server IBAN should not be offered save at all.
TEST_F(IbanSaveManagerTest, ShouldOfferUploadSave_ServerIban) {
  Iban iban(Iban::InstrumentId(1234567));
  iban.set_prefix(u"DE91");
  iban.set_suffix(u"6789");
  iban.set_length(22);
  personal_data().AddServerIban(iban);

  // Creates an unknown IBAN with the same prefix, suffix and length as the
  // above server IBAN.
  Iban another_iban;
  another_iban.set_value(u"DE91100000000123456789");
  EXPECT_EQ(IbanSaveManager::TypeOfOfferToSave::kDoNotOfferToSave,
            GetIbanSaveManager().DetermineHowToSaveIbanForTesting(iban));
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Accepted) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kAccepted,
      u"  My teacher's IBAN ");
  const std::vector<const Iban*> ibans = personal_data().GetLocalIbans();

  // Verify IBAN has been successfully updated with the new nickname on accept.
  ASSERT_EQ(ibans.size(), 1U);
  EXPECT_EQ(ibans[0]->value(), iban.value());
  EXPECT_EQ(ibans[0]->nickname(), u"My teacher's IBAN");
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Declined) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  EXPECT_TRUE(iban_save_manager_->AttemptToOfferLocalSaveForTesting(iban));
  EXPECT_TRUE(personal_data().GetLocalIbans().empty());

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kDeclined);
  const std::vector<const Iban*> ibans = personal_data().GetLocalIbans();

  EXPECT_TRUE(personal_data().GetLocalIbans().empty());
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Ignored) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  EXPECT_TRUE(iban_save_manager_->AttemptToOfferLocalSaveForTesting(iban));
  EXPECT_TRUE(personal_data().GetLocalIbans().empty());

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kIgnored);
  const std::vector<const Iban*> ibans = personal_data().GetLocalIbans();

  EXPECT_TRUE(personal_data().GetLocalIbans().empty());
}

TEST_F(IbanSaveManagerTest, LocallySaveIban_NotEnoughStrikesShouldOfferToSave) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  const std::string partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(partial_iban_hash);

  // Verify `kIbanValue` has been successfully added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));
}

TEST_F(IbanSaveManagerTest, LocallySaveIban_MaxStrikesShouldNotOfferToSave) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  const std::string partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));
  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrikes(
      iban_save_strike_database.GetMaxStrikesLimit(), partial_iban_hash);

  EXPECT_EQ(iban_save_strike_database.GetMaxStrikesLimit(),
            iban_save_strike_database.GetStrikes(partial_iban_hash));
  EXPECT_FALSE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Accepted_ClearsStrikes) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  const std::string partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));

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
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  const std::string partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));

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
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  const std::string partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));

  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);

  EXPECT_EQ(0, iban_save_strike_database.GetStrikes(partial_iban_hash));

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kDeclined,
      u"My teacher's IBAN");

  // Verify partial hashed value of `partial_iban_hash` has been
  // added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IbanSaveManagerTest, LocallySaveIban_OfferIbanSave) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));
  EXPECT_TRUE(autofill_client_.ConfirmSaveIbanLocallyWasCalled());
}

TEST_F(IbanSaveManagerTest,
       LocallySaveIban_MaxStrikesShouldNotOfferToSave_Metrics) {
  base::HistogramTester histogram_tester;
  Iban iban;
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
  EXPECT_FALSE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.IbanSaveNotOfferedDueToMaxStrikes",
      AutofillMetrics::SaveTypeMetric::LOCAL, 1);
}

TEST_F(IbanSaveManagerTest, StrikesPresentWhenIbanSaved_Local) {
  base::HistogramTester histogram_tester;
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(IbanSaveManager::GetPartialIbanHashString(
      test::GetStrippedValue(test::kIbanValue)));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));
  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      AutofillClient::SaveIbanOfferUserDecision::kAccepted,
      u"My teacher's IBAN");

  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.StrikesPresentWhenIbanSaved.Local",
      /*sample=*/1, /*expected_count=*/1);
}

TEST_F(IbanSaveManagerTest, IsIbanUploadEnabled_SyncServiceNotAvailable) {
  EXPECT_FALSE(IbanSaveManager::IsIbanUploadEnabled(/*sync_service=*/nullptr));
}

TEST_F(IbanSaveManagerTest, IsIbanUploadEnabled_AuthError) {
  // Set the SyncService to paused state.
  sync_service_.SetPersistentAuthError();
  EXPECT_FALSE(IbanSaveManager::IsIbanUploadEnabled(&sync_service_));
}

TEST_F(IbanSaveManagerTest,
       IsIbanUploadEnabled_SyncDoesNotHaveAutofillWalletDataActiveType) {
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());
  EXPECT_FALSE(IbanSaveManager::IsIbanUploadEnabled(&sync_service_));
}

TEST_F(IbanSaveManagerTest,
       IsIbanUploadEnabled_SyncServiceUsingExplicitPassphrase) {
  sync_service_.SetIsUsingExplicitPassphrase(true);
  EXPECT_FALSE(IbanSaveManager::IsIbanUploadEnabled(&sync_service_));
}

TEST_F(IbanSaveManagerTest, IsIbanUploadEnabled_SyncServiceLocalSyncOnly) {
  sync_service_.SetLocalSyncEnabled(true);
  EXPECT_FALSE(IbanSaveManager::IsIbanUploadEnabled(&sync_service_));
}

TEST_F(IbanSaveManagerTest, IsIbanUploadEnabled_Enabled) {
  EXPECT_TRUE(IbanSaveManager::IsIbanUploadEnabled(&sync_service_));
}

// Test that upload save should be offered to a new IBAN when the preflight
// call succeeded and the `legal_message` is parsed successfully.
TEST_F(IbanSaveManagerTest, OfferUploadSave_NewIban_Success) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));
  EXPECT_TRUE(GetIbanSaveManager().HasContextTokenForTesting());
  EXPECT_TRUE(autofill_client_.ConfirmUploadIbanToCloudWasCalled());
  EXPECT_FALSE(autofill_client_.ConfirmSaveIbanLocallyWasCalled());
}

// Test that upload save should not be offered when the preflight call failed.
// In this case, local save should be offered because the extracted IBAN is a
// new IBAN.
TEST_F(IbanSaveManagerTest,
       OfferUploadSave_NewIban_Failure_ThenAttemptToOfferLocalSave) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));

  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/false);

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));
  EXPECT_FALSE(GetIbanSaveManager().HasContextTokenForTesting());
  EXPECT_FALSE(autofill_client_.ConfirmUploadIbanToCloudWasCalled());
  EXPECT_TRUE(autofill_client_.ConfirmSaveIbanLocallyWasCalled());
}

// Test that upload save should not be offered when the preflight call succeeded
// but the `legal_message` is not parsed successfully. In this case, local save
// should be offered because the extracted IBAN is a new IBAN.
TEST_F(
    IbanSaveManagerTest,
    OfferUploadSave_NewIban_InvalidLegalMessage_ThenAttemptToOfferLocalSave) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));

  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true,
                                    /*includes_invalid_legal_message=*/true);

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));
  EXPECT_FALSE(GetIbanSaveManager().HasContextTokenForTesting());
  EXPECT_FALSE(autofill_client_.ConfirmUploadIbanToCloudWasCalled());
  EXPECT_TRUE(autofill_client_.ConfirmSaveIbanLocallyWasCalled());
}

// Test that upload save should be offered to a local IBAN when the preflight
// call succeeded and the `legal_message` is parsed successfully.
TEST_F(IbanSaveManagerTest, OfferUploadSave_LocalIban_Success) {
  Iban local_iban;
  local_iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  personal_data().AddAsLocalIban(local_iban);
  Iban another_iban;
  another_iban.set_value(local_iban.value());

  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);

  EXPECT_TRUE(
      GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(another_iban));
  EXPECT_TRUE(GetIbanSaveManager().HasContextTokenForTesting());
  EXPECT_TRUE(autofill_client_.ConfirmUploadIbanToCloudWasCalled());
  EXPECT_FALSE(autofill_client_.ConfirmSaveIbanLocallyWasCalled());
}

// Test that upload save should not be offered when the preflight call failed
// and the `legal_message` is parsed successfully. Then Local save should not be
// offered because the extracted IBAN already exists.
TEST_F(IbanSaveManagerTest,
       OfferUploadSave_LocalIban_Failure_LocalSaveNotOffered) {
  Iban local_iban;
  local_iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  personal_data().AddAsLocalIban(local_iban);
  Iban another_iban;
  another_iban.set_value(local_iban.value());

  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/false);

  EXPECT_TRUE(
      GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(another_iban));
  EXPECT_FALSE(GetIbanSaveManager().HasContextTokenForTesting());
  EXPECT_FALSE(autofill_client_.ConfirmUploadIbanToCloudWasCalled());
  EXPECT_FALSE(autofill_client_.ConfirmSaveIbanLocallyWasCalled());
}

TEST_F(IbanSaveManagerTest, UploadSaveIban_Accept_SuccessShouldClearStrikes) {
  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);
  SetUpUploadIbanResponse(/*is_successful=*/true);
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  const std::string& partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));
  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(partial_iban_hash);

  // Verify `kIbanValue` has been successfully added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));

  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      /*show_save_prompt=*/true,
      AutofillClient::SaveIbanOfferUserDecision::kAccepted,
      u"My teacher's IBAN");

  // Verify the IBAN's strikes have been cleared.
  EXPECT_EQ(0, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IbanSaveManagerTest, UploadSaveIban_Accept_FailureShouldAddStrike) {
  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);
  SetUpUploadIbanResponse(/*is_successful=*/false);
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  const std::string& partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));
  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(partial_iban_hash);

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));

  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      /*show_save_prompt=*/true,
      AutofillClient::SaveIbanOfferUserDecision::kAccepted,
      u"My teacher's IBAN");

  // Verify the IBAN's strikes have been added by 1.
  EXPECT_EQ(2, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnUploadSave_Decline_AddsStrike) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  const std::string& partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));

  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      /*show_save_prompt=*/true,
      AutofillClient::SaveIbanOfferUserDecision::kDeclined);

  // Verify the IBAN's strikes have been added by 1.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
  // Verify that `UploadIban` handler is not triggered.
  EXPECT_CALL(*payments_network_interface(), UploadIban).Times(0);
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnUploadSave_Ignore_AddsStrike) {
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  const std::string& partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));

  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      /*show_save_prompt=*/true,
      AutofillClient::SaveIbanOfferUserDecision::kIgnored);

  // Verify the IBAN's strikes have been added by 1.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
  // Verify that `UploadIban` handler is not triggered.
  EXPECT_CALL(*payments_network_interface(), UploadIban).Times(0);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace autofill
