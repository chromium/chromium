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
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/strike_databases/payments/iban_save_strike_database.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

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

constexpr char16_t kCapitalizedIbanRegex[] =
    u"^[A-Z]{2}[0-9]{2}[A-Z0-9]{4}[0-9]{7}[A-Z0-9]{0,18}$";

class IbanSaveManagerTest : public testing::Test {
 public:
  IbanSaveManagerTest() {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    autofill_client_.set_personal_data_manager(
        std::make_unique<TestPersonalDataManager>());
    autofill_client_.GetPaymentsAutofillClient()
        ->set_test_payments_network_interface(
            std::make_unique<MockTestPaymentsNetworkInterface>());
    autofill_client_.set_sync_service(&sync_service_);
    std::unique_ptr<TestStrikeDatabase> test_strike_database =
        std::make_unique<TestStrikeDatabase>();
    strike_database_ = test_strike_database.get();
    autofill_client_.set_test_strike_database(std::move(test_strike_database));
    personal_data().payments_data_manager().SetSyncingForTest(true);
    personal_data().SetPrefService(autofill_client_.GetPrefs());
    iban_save_manager_ = std::make_unique<IbanSaveManager>(&autofill_client_);
  }

  using SaveIbanOfferUserDecision =
      payments::PaymentsAutofillClient::SaveIbanOfferUserDecision;

  IbanSaveManager& GetIbanSaveManager() { return *iban_save_manager_; }

  void SetUpGetIbanUploadDetailsResponse(
      bool is_successful,
      const std::u16string& regex = kCapitalizedIbanRegex,
      bool includes_invalid_legal_message = false) {
    ON_CALL(*payments_network_interface(), GetIbanUploadDetails)
        .WillByDefault(
            [is_successful, regex, includes_invalid_legal_message](
                const std::string& app_locale, int64_t billing_customer_number,
                int billable_service_number, const std::string& country_code,
                base::OnceCallback<void(
                    payments::PaymentsAutofillClient::PaymentsRpcResult,
                    const std::u16string& validation_regex,
                    const std::u16string& context_token,
                    std::unique_ptr<base::Value::Dict>)> callback) {
              std::move(callback).Run(
                  is_successful ? payments::PaymentsAutofillClient::
                                      PaymentsRpcResult::kSuccess
                                : payments::PaymentsAutofillClient::
                                      PaymentsRpcResult::kPermanentFailure,
                  regex, u"this is a context token",
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
                base::OnceCallback<void(
                    payments::PaymentsAutofillClient::PaymentsRpcResult)>
                    callback) {
              std::move(callback).Run(
                  is_successful ? payments::PaymentsAutofillClient::
                                      PaymentsRpcResult::kSuccess
                                : payments::PaymentsAutofillClient::
                                      PaymentsRpcResult::kPermanentFailure);
            });
  }

 protected:
  TestPersonalDataManager& personal_data() {
    return static_cast<TestPersonalDataManager&>(
        *autofill_client_.GetPersonalDataManager());
  }

  MockTestPaymentsNetworkInterface* payments_network_interface() {
    return static_cast<MockTestPaymentsNetworkInterface*>(
        autofill_client_.GetPaymentsAutofillClient()
            ->GetPaymentsNetworkInterface());
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
  iban.set_value(std::u16string(test::kIbanValue16));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));
}

TEST_F(IbanSaveManagerTest, AttemptToOfferSave_LocalIban_ShouldOfferSave) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  personal_data().payments_data_manager().AddAsLocalIban(iban);

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
  iban.set_value(std::u16string(test::kIbanValue16));
  EXPECT_EQ(IbanSaveManager::TypeOfOfferToSave::kOfferLocalSave,
            GetIbanSaveManager().DetermineHowToSaveIbanForTesting(iban));
}

// Test that new IBANs should not be offered upload save due to reaching the
// maximum limit.
TEST_F(IbanSaveManagerTest, ShouldOfferUploadSave_MaxServerIban) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  for (int num_server_ibans = 1; num_server_ibans <= kMaxNumServerIbans + 1;
       num_server_ibans++) {
    Iban server_iban((Iban::InstrumentId(num_server_ibans)));
    server_iban.set_prefix(u"DE");
    server_iban.set_suffix(
        (base::UTF8ToUTF16(base::NumberToString(10 + num_server_ibans))));
    personal_data().test_payments_data_manager().AddServerIban(server_iban);
    EXPECT_EQ(num_server_ibans <= kMaxNumServerIbans
                  ? IbanSaveManager::TypeOfOfferToSave::kOfferServerSave
                  : IbanSaveManager::TypeOfOfferToSave::kOfferLocalSave,
              GetIbanSaveManager().DetermineHowToSaveIbanForTesting(iban));
  }
}

// Test that new IBANs should be offered upload save to Google Payments.
TEST_F(IbanSaveManagerTest, ShouldOfferUploadSave_NewIban) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  EXPECT_EQ(IbanSaveManager::TypeOfOfferToSave::kOfferServerSave,
            GetIbanSaveManager().DetermineHowToSaveIbanForTesting(iban));
}

// Test that an existing local IBAN should still be offered upload save to
// Google Payments.
TEST_F(IbanSaveManagerTest, ShouldOfferUploadSave_LocalIban) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  personal_data().payments_data_manager().AddAsLocalIban(iban);

  Iban another_iban;
  another_iban.set_value(iban.value());
  EXPECT_EQ(IbanSaveManager::TypeOfOfferToSave::kOfferServerSave,
            GetIbanSaveManager().DetermineHowToSaveIbanForTesting(iban));
}

// Test that an existing local and server IBAN should not be offered save at
// all.
TEST_F(IbanSaveManagerTest, ShouldOfferUploadSave_LocalAndServerIban) {
  Iban local_iban;
  local_iban.set_value(u"DE91100000000123456789");
  personal_data().payments_data_manager().AddAsLocalIban(local_iban);
  Iban server_iban(Iban::InstrumentId(1234567));
  server_iban.set_prefix(u"DE91");
  server_iban.set_suffix(u"6789");
  personal_data().test_payments_data_manager().AddServerIban(server_iban);

  // Creates an unknown IBAN with the same prefix, suffix and length as the
  // above server IBAN.
  Iban iban;
  iban.set_value(u"DE91100000000123456789");
  EXPECT_FALSE(GetIbanSaveManager().AttemptToOfferSave(iban));
}

// Test that an existing server IBAN should not be offered save at all.
TEST_F(IbanSaveManagerTest, ShouldOfferUploadSave_ServerIban) {
  Iban iban(Iban::InstrumentId(1234567));
  iban.set_prefix(u"DE91");
  iban.set_suffix(u"6789");
  personal_data().test_payments_data_manager().AddServerIban(iban);

  // Creates an unknown IBAN with the same prefix, suffix and length as the
  // above server IBAN.
  Iban another_iban;
  another_iban.set_value(u"DE91100000000123456789");
  EXPECT_EQ(IbanSaveManager::TypeOfOfferToSave::kDoNotOfferToSave,
            GetIbanSaveManager().DetermineHowToSaveIbanForTesting(iban));
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Accepted) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      iban, SaveIbanOfferUserDecision::kAccepted, u"  My teacher's IBAN ");
  const std::vector<const Iban*> ibans =
      personal_data().payments_data_manager().GetLocalIbans();

  // Verify IBAN has been successfully updated with the new nickname on accept.
  ASSERT_EQ(ibans.size(), 1U);
  EXPECT_EQ(ibans[0]->value(), iban.value());
  EXPECT_EQ(ibans[0]->nickname(), u"My teacher's IBAN");
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Declined) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  EXPECT_TRUE(iban_save_manager_->AttemptToOfferLocalSaveForTesting(iban));
  EXPECT_TRUE(personal_data().payments_data_manager().GetLocalIbans().empty());

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      iban, SaveIbanOfferUserDecision::kDeclined);
  const std::vector<const Iban*> ibans =
      personal_data().payments_data_manager().GetLocalIbans();

  EXPECT_TRUE(personal_data().payments_data_manager().GetLocalIbans().empty());
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Ignored) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  EXPECT_TRUE(iban_save_manager_->AttemptToOfferLocalSaveForTesting(iban));
  EXPECT_TRUE(personal_data().payments_data_manager().GetLocalIbans().empty());

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      iban, SaveIbanOfferUserDecision::kIgnored);
  const std::vector<const Iban*> ibans =
      personal_data().payments_data_manager().GetLocalIbans();

  EXPECT_TRUE(personal_data().payments_data_manager().GetLocalIbans().empty());
}

TEST_F(IbanSaveManagerTest, LocallySaveIban_NotEnoughStrikesShouldOfferToSave) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
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
  iban.set_value(std::u16string(test::kIbanValue16));
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
  iban.set_value(std::u16string(test::kIbanValue16));
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
      iban, SaveIbanOfferUserDecision::kAccepted, u"My teacher's IBAN");

  // Verify partial hashed value of `partial_iban_hash` has been
  // cleared in the strike database.
  EXPECT_EQ(0, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Declined_AddsStrike) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  const std::string partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));

  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);

  EXPECT_EQ(0, iban_save_strike_database.GetStrikes(partial_iban_hash));

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      iban, SaveIbanOfferUserDecision::kDeclined, u"My teacher's IBAN");

  // Verify partial hashed value of `partial_iban_hash` has been
  // added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnLocalSave_Ignored_AddsStrike) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  const std::string partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));

  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);

  EXPECT_EQ(0, iban_save_strike_database.GetStrikes(partial_iban_hash));

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      iban, SaveIbanOfferUserDecision::kDeclined, u"My teacher's IBAN");

  // Verify partial hashed value of `partial_iban_hash` has been
  // added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IbanSaveManagerTest, LocallySaveIban_OfferIbanSave) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->ConfirmSaveIbanLocallyWasCalled());
}

TEST_F(IbanSaveManagerTest,
       LocallySaveIban_MaxStrikesShouldNotOfferToSave_Metrics) {
  base::HistogramTester histogram_tester;
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
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
  iban.set_value(std::u16string(test::kIbanValue16));
  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(IbanSaveManager::GetPartialIbanHashString(
      test::GetStrippedValue(test::kIbanValue)));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));
  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      iban, SaveIbanOfferUserDecision::kAccepted, u"My teacher's IBAN");

  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.StrikesPresentWhenIbanSaved.Local",
      /*sample=*/1, /*expected_count=*/1);
}

TEST_F(IbanSaveManagerTest, IsIbanUploadEnabled_SyncServiceNotAvailable) {
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(IbanSaveManager::IsIbanUploadEnabled(
      /*sync_service=*/nullptr,
      AutofillMetrics::PaymentsSigninState::kSignedOut));
  histogram_tester.ExpectUniqueSample(
      "Autofill.IbanUploadEnabled",
      autofill_metrics::IbanUploadEnabledStatus::kSyncServiceNull, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.IbanUploadEnabled.SignedOut",
      autofill_metrics::IbanUploadEnabledStatus::kSyncServiceNull, 1);
}

TEST_F(IbanSaveManagerTest, IsIbanUploadEnabled_AuthError) {
  base::HistogramTester histogram_tester;
  // Set the SyncService to paused state.
  sync_service_.SetPersistentAuthError();

  EXPECT_FALSE(IbanSaveManager::IsIbanUploadEnabled(
      &sync_service_, AutofillMetrics::PaymentsSigninState::kSyncPaused));
  histogram_tester.ExpectUniqueSample(
      "Autofill.IbanUploadEnabled",
      autofill_metrics::IbanUploadEnabledStatus::kSyncServicePaused, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.IbanUploadEnabled.SyncPaused",
      autofill_metrics::IbanUploadEnabledStatus::kSyncServicePaused, 1);
}

TEST_F(IbanSaveManagerTest,
       IsIbanUploadEnabled_SyncDoesNotHaveAutofillWalletDataActiveType) {
  base::HistogramTester histogram_tester;
  sync_service_.GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());

  EXPECT_FALSE(IbanSaveManager::IsIbanUploadEnabled(
      &sync_service_,
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.IbanUploadEnabled",
      autofill_metrics::IbanUploadEnabledStatus::
          kSyncServiceMissingAutofillWalletDataActiveType,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.IbanUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::IbanUploadEnabledStatus::
          kSyncServiceMissingAutofillWalletDataActiveType,
      1);
}

TEST_F(IbanSaveManagerTest,
       IsIbanUploadEnabled_SyncServiceUsingExplicitPassphrase) {
  base::HistogramTester histogram_tester;
  sync_service_.SetIsUsingExplicitPassphrase(true);

  EXPECT_FALSE(IbanSaveManager::IsIbanUploadEnabled(
      &sync_service_,
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.IbanUploadEnabled",
      autofill_metrics::IbanUploadEnabledStatus::kUsingExplicitSyncPassphrase,
      1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.IbanUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::IbanUploadEnabledStatus::kUsingExplicitSyncPassphrase,
      1);
}

TEST_F(IbanSaveManagerTest, IsIbanUploadEnabled_SyncServiceLocalSyncOnly) {
  base::HistogramTester histogram_tester;
  sync_service_.SetLocalSyncEnabled(true);

  EXPECT_FALSE(IbanSaveManager::IsIbanUploadEnabled(
      &sync_service_,
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.IbanUploadEnabled",
      autofill_metrics::IbanUploadEnabledStatus::kLocalSyncEnabled, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.IbanUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::IbanUploadEnabledStatus::kLocalSyncEnabled, 1);
}

TEST_F(IbanSaveManagerTest, IsIbanUploadEnabled_Enabled) {
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(IbanSaveManager::IsIbanUploadEnabled(
      &sync_service_,
      AutofillMetrics::PaymentsSigninState::kSignedInAndSyncFeatureEnabled));
  histogram_tester.ExpectUniqueSample(
      "Autofill.IbanUploadEnabled",
      autofill_metrics::IbanUploadEnabledStatus::kEnabled, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.IbanUploadEnabled.SignedInAndSyncFeatureEnabled",
      autofill_metrics::IbanUploadEnabledStatus::kEnabled, 1);
}

// Test that upload save should be offered to a new IBAN when the preflight
// call succeeded and the `legal_message` is parsed successfully.
TEST_F(IbanSaveManagerTest, OfferUploadSave_NewIban_Success) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));
  EXPECT_TRUE(GetIbanSaveManager().HasContextTokenForTesting());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->ConfirmUploadIbanToCloudWasCalled());
  EXPECT_FALSE(autofill_client_.GetPaymentsAutofillClient()
                   ->ConfirmSaveIbanLocallyWasCalled());
}

// Test that upload save should be not be offered for a new IBAN when the
// regex validation does not pass.
TEST_F(IbanSaveManagerTest, OfferUploadSave_NewIban_FailureOnRegexNotMatch) {
  Iban iban;
  // Set up a valid France IBAN value.
  iban.set_value(std::u16string(test::kIbanValue16));
  // Set up a Finland validation regex.
  SetUpGetIbanUploadDetailsResponse(
      /*is_successful=*/true,
      u"^FI{2}[0-9]{2}[A-Z0-9]{4}[0-9]{7}[A-Z0-9]{0,18}$",
      /*includes_invalid_legal_message=*/false);

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));
  EXPECT_FALSE(autofill_client_.GetPaymentsAutofillClient()
                   ->ConfirmUploadIbanToCloudWasCalled());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->ConfirmSaveIbanLocallyWasCalled());
}

// Test that upload save should not be offered when the preflight call failed.
// In this case, local save should be offered because the extracted IBAN is a
// new IBAN.
TEST_F(IbanSaveManagerTest,
       OfferUploadSave_NewIban_Failure_ThenAttemptToOfferLocalSave) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));

  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/false);

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));
  EXPECT_FALSE(GetIbanSaveManager().HasContextTokenForTesting());
  EXPECT_FALSE(autofill_client_.GetPaymentsAutofillClient()
                   ->ConfirmUploadIbanToCloudWasCalled());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->ConfirmSaveIbanLocallyWasCalled());
}

// Test that upload save should not be offered when the preflight call succeeded
// but the `legal_message` is not parsed successfully. In this case, local save
// should be offered because the extracted IBAN is a new IBAN.
TEST_F(
    IbanSaveManagerTest,
    OfferUploadSave_NewIban_InvalidLegalMessage_ThenAttemptToOfferLocalSave) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));

  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true,
                                    kCapitalizedIbanRegex,
                                    /*includes_invalid_legal_message=*/true);

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));
  EXPECT_FALSE(GetIbanSaveManager().HasContextTokenForTesting());
  EXPECT_FALSE(autofill_client_.GetPaymentsAutofillClient()
                   ->ConfirmUploadIbanToCloudWasCalled());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->ConfirmSaveIbanLocallyWasCalled());
}

// call succeeded and the `legal_message` is parsed successfully.
TEST_F(IbanSaveManagerTest, OfferUploadSave_LocalIban_Success) {
  Iban local_iban;
  local_iban.set_value(std::u16string(test::kIbanValue16));
  personal_data().payments_data_manager().AddAsLocalIban(local_iban);
  Iban another_iban;
  another_iban.set_value(local_iban.value());

  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);

  EXPECT_TRUE(
      GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(another_iban));
  EXPECT_TRUE(GetIbanSaveManager().HasContextTokenForTesting());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->ConfirmUploadIbanToCloudWasCalled());
  EXPECT_FALSE(autofill_client_.GetPaymentsAutofillClient()
                   ->ConfirmSaveIbanLocallyWasCalled());
}

// Test that upload save should not be offered when the preflight call failed
// and the `legal_message` is parsed successfully. Then Local save should not be
// offered because the extracted IBAN already exists.
TEST_F(IbanSaveManagerTest,
       OfferUploadSave_LocalIban_Failure_LocalSaveNotOffered) {
  Iban local_iban;
  local_iban.set_value(std::u16string(test::kIbanValue16));
  personal_data().payments_data_manager().AddAsLocalIban(local_iban);
  Iban another_iban;
  another_iban.set_value(local_iban.value());

  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/false);

  EXPECT_TRUE(
      GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(another_iban));
  EXPECT_FALSE(GetIbanSaveManager().HasContextTokenForTesting());
  EXPECT_FALSE(autofill_client_.GetPaymentsAutofillClient()
                   ->ConfirmUploadIbanToCloudWasCalled());
  EXPECT_FALSE(autofill_client_.GetPaymentsAutofillClient()
                   ->ConfirmSaveIbanLocallyWasCalled());
}

TEST_F(IbanSaveManagerTest, UploadSaveIban_Accept_SuccessShouldClearStrikes) {
  base::HistogramTester histogram_tester;
  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);
  SetUpUploadIbanResponse(/*is_successful=*/true);
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  const std::string& partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));
  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(partial_iban_hash);

  // Verify `kIbanValue` has been successfully added to the strike database.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));

  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      iban, /*show_save_prompt=*/true, SaveIbanOfferUserDecision::kAccepted,
      u"My teacher's IBAN");

  // Verify the IBAN's strikes have been cleared.
  EXPECT_EQ(0, iban_save_strike_database.GetStrikes(partial_iban_hash));
  histogram_tester.ExpectBucketCount(
      "Autofill.StrikeDatabase.StrikesPresentWhenIbanSaved.Upload",
      /*sample=*/1, /*expected_count=*/1);
}

TEST_F(IbanSaveManagerTest, UploadSaveIban_Accept_FailureShouldAddStrike) {
  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);
  SetUpUploadIbanResponse(/*is_successful=*/false);
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  const std::string& partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));
  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  iban_save_strike_database.AddStrike(partial_iban_hash);

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));

  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      iban, /*show_save_prompt=*/true, SaveIbanOfferUserDecision::kAccepted,
      u"My teacher's IBAN");

  // Verify the IBAN's strikes have been added by 1.
  EXPECT_EQ(2, iban_save_strike_database.GetStrikes(partial_iban_hash));
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnUploadSave_Decline_AddsStrike) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  const std::string& partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));

  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      iban, /*show_save_prompt=*/true, SaveIbanOfferUserDecision::kDeclined);

  // Verify the IBAN's strikes have been added by 1.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
  // Verify that `UploadIban` handler is not triggered.
  EXPECT_CALL(*payments_network_interface(), UploadIban).Times(0);
}

TEST_F(IbanSaveManagerTest, OnUserDidDecideOnUploadSave_Ignore_AddsStrike) {
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  const std::string& partial_iban_hash =
      IbanSaveManager::GetPartialIbanHashString(
          base::UTF16ToUTF8(iban.value()));
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferUploadSaveForTesting(iban));

  IbanSaveStrikeDatabase iban_save_strike_database(strike_database_);
  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      iban, /*show_save_prompt=*/true, SaveIbanOfferUserDecision::kIgnored);

  // Verify the IBAN's strikes have been added by 1.
  EXPECT_EQ(1, iban_save_strike_database.GetStrikes(partial_iban_hash));
  // Verify that `UploadIban` handler is not triggered.
  EXPECT_CALL(*payments_network_interface(), UploadIban).Times(0);
}

// Test that offer-to-upload IBAN origin is logged as `kNewIban` for a new IBAN.
TEST_F(IbanSaveManagerTest, Metric_UploadOfferedIbanOrigin_NewIban) {
  base::HistogramTester histogram_tester;
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  ASSERT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));

  histogram_tester.ExpectBucketCount(
      "Autofill.UploadIban.Offered",
      /*sample=*/
      autofill_metrics::UploadIbanOriginMetric::kNewIban,
      /*expected_count=*/1);
}

// Test that offer-to-upload IBAN origin is logged as `kLocalIban` for a local
// IBAN.
TEST_F(IbanSaveManagerTest, Metric_UploadOfferedIbanOrigin_LocalIban) {
  base::HistogramTester histogram_tester;
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  personal_data().payments_data_manager().AddAsLocalIban(iban);

  ASSERT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));

  histogram_tester.ExpectBucketCount(
      "Autofill.UploadIban.Offered",
      /*sample=*/
      autofill_metrics::UploadIbanOriginMetric::kLocalIban,
      /*expected_count=*/1);
}

// Test that when upload is offered for a new IBAN and the offer is accepted,
// `kAccepted` and `kNewIban` are logged.
TEST_F(IbanSaveManagerTest, Metric_AcceptedOfferedIbanOrigin_NewIban) {
  base::HistogramTester histogram_tester;
  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);
  SetUpUploadIbanResponse(/*is_successful=*/true);
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));

  ASSERT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));
  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      iban, /*show_save_prompt=*/true, SaveIbanOfferUserDecision::kAccepted,
      u"My teacher's IBAN");

  histogram_tester.ExpectBucketCount(
      "Autofill.UploadIban.Accepted",
      /*sample=*/
      autofill_metrics::UploadIbanOriginMetric::kNewIban,
      /*expected_count=*/1);
}

// Test that when upload is offered for an existing IBAN and the offer is
// accepted, `kAccepted` and `kLocalIban` are logged.
TEST_F(IbanSaveManagerTest, Metric_AcceptedOfferedIbanOrigin_LocalIban) {
  base::HistogramTester histogram_tester;
  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);
  SetUpUploadIbanResponse(/*is_successful=*/true);
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  personal_data().payments_data_manager().AddAsLocalIban(iban);

  ASSERT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));
  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      iban, /*show_save_prompt=*/true, SaveIbanOfferUserDecision::kAccepted,
      u"My teacher's IBAN");

  histogram_tester.ExpectBucketCount(
      "Autofill.UploadIban.Accepted",
      /*sample=*/
      autofill_metrics::UploadIbanOriginMetric::kLocalIban,
      /*expected_count=*/1);
}

// Test that when upload is offered for a new IBAN and the offer is declined,
// `kDeclined` and `kNewIban` are logged.
TEST_F(IbanSaveManagerTest, Metric_DeclinedOfferedIbanOrigin_NewIban) {
  base::HistogramTester histogram_tester;
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));

  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      iban, /*show_save_prompt=*/true, SaveIbanOfferUserDecision::kDeclined,
      u"My teacher's IBAN");

  histogram_tester.ExpectBucketCount(
      "Autofill.UploadIban.Declined",
      /*sample=*/autofill_metrics::UploadIbanOriginMetric::kNewIban,
      /*expected_count=*/1);
}

// Test that when upload is offered for an existing IBAN and the offer is
// declined, `kDeclined` and `kLocalIban` are logged.
TEST_F(IbanSaveManagerTest, Metric_DeclinedOfferedIbanOrigin_LocalIban) {
  base::HistogramTester histogram_tester;
  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);
  SetUpUploadIbanResponse(/*is_successful=*/true);
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  personal_data().payments_data_manager().AddAsLocalIban(iban);

  ASSERT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));
  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      iban, /*show_save_prompt=*/true, SaveIbanOfferUserDecision::kDeclined,
      u"My teacher's IBAN");

  histogram_tester.ExpectBucketCount(
      "Autofill.UploadIban.Declined",
      /*sample=*/
      autofill_metrics::UploadIbanOriginMetric::kLocalIban,
      /*expected_count=*/1);
}

// Test that when upload is offered for a new IBAN and the offer is ignored,
// `kIgnored` and `kNewIban` are logged.
TEST_F(IbanSaveManagerTest, Metric_IgnoredOfferedIbanOrigin_NewIban) {
  base::HistogramTester histogram_tester;
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));

  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      iban, /*show_save_prompt=*/true, SaveIbanOfferUserDecision::kIgnored,
      u"My teacher's IBAN");

  histogram_tester.ExpectBucketCount(
      "Autofill.UploadIban.Ignored",
      /*sample=*/autofill_metrics::UploadIbanOriginMetric::kNewIban,
      /*expected_count=*/1);
}

// Test that when upload is offered for an existing IBAN and the offer is
// ignored, `kIgnored` and `kLocalIban` are logged.
TEST_F(IbanSaveManagerTest, Metric_IgnoredOfferedIbanOrigin_LocalIban) {
  base::HistogramTester histogram_tester;
  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);
  SetUpUploadIbanResponse(/*is_successful=*/true);
  Iban iban;
  iban.set_value(base::UTF8ToUTF16(std::string(test::kIbanValue)));
  personal_data().payments_data_manager().AddAsLocalIban(iban);

  ASSERT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));
  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      iban, /*show_save_prompt=*/true, SaveIbanOfferUserDecision::kIgnored,
      u"My teacher's IBAN");

  histogram_tester.ExpectBucketCount(
      "Autofill.UploadIban.Ignored",
      /*sample=*/
      autofill_metrics::UploadIbanOriginMetric::kLocalIban,
      /*expected_count=*/1);
}

TEST_F(IbanSaveManagerTest, Metric_CountryOfSaveOffered_LocalIban) {
  base::test::ScopedFeatureList disable_server_iban;
  disable_server_iban.InitAndDisableFeature(
      features::kAutofillEnableServerIban);
  base::HistogramTester histogram_tester;
  Iban iban;
  iban.set_value(u"FR7630006000011234567890189");
  ASSERT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));

  histogram_tester.ExpectUniqueSample("Autofill.Iban.CountryOfSaveOfferedIban",
                                      Iban::IbanSupportedCountry::kFR, 1);
}

TEST_F(IbanSaveManagerTest, Metric_CountryOfSaveOffered_ServerIban) {
  base::HistogramTester histogram_tester;
  Iban iban;
  iban.set_value(u"FR7630006000011234567890189");
  ASSERT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));

  histogram_tester.ExpectUniqueSample("Autofill.Iban.CountryOfSaveOfferedIban",
                                      Iban::IbanSupportedCountry::kFR, 1);
}

TEST_F(IbanSaveManagerTest, Metric_CountryOfSaveAccepted_LocalIban) {
  base::HistogramTester histogram_tester;
  Iban iban;
  iban.set_value(u"FR7630006000011234567890189");
  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferLocalSaveForTesting(iban));

  GetIbanSaveManager().OnUserDidDecideOnLocalSaveForTesting(
      iban, SaveIbanOfferUserDecision::kAccepted, u"IBAN nickname");

  histogram_tester.ExpectUniqueSample("Autofill.Iban.CountryOfSaveAcceptedIban",
                                      Iban::IbanSupportedCountry::kFR, 1);
}

TEST_F(IbanSaveManagerTest, Metric_CountryOfSaveAccepted_ServerIban) {
  base::HistogramTester histogram_tester;
  SetUpGetIbanUploadDetailsResponse(/*is_successful=*/true);
  SetUpUploadIbanResponse(/*is_successful=*/true);
  Iban iban;
  iban.set_value(u"FR7630006000011234567890189");

  EXPECT_TRUE(GetIbanSaveManager().AttemptToOfferSave(iban));
  GetIbanSaveManager().OnUserDidDecideOnUploadSaveForTesting(
      iban, /*show_save_prompt=*/true, SaveIbanOfferUserDecision::kAccepted,
      u"IBAN nickname");

  histogram_tester.ExpectUniqueSample("Autofill.Iban.CountryOfSaveAcceptedIban",
                                      Iban::IbanSupportedCountry::kFR, 1);
}

// Tests that the `RanLocalSaveFallback` metric records that a new IBAN was
// saved during the local IBAN save fallback for a server upload failure.
TEST_F(IbanSaveManagerTest,
       Metrics_OnUploadIban_FallbackToLocalSave_NewIbanAdded) {
  base::HistogramTester histogram_tester;
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  ASSERT_TRUE(personal_data().payments_data_manager().GetLocalIbans().empty());
  GetIbanSaveManager().OnDidUploadIbanForTesting(
      iban, /*show_save_prompt=*/true,
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure);

  histogram_tester.ExpectUniqueSample("Autofill.IbanUpload.SaveFailed", true,
                                      1);
  EXPECT_EQ(personal_data().payments_data_manager().GetLocalIbans().size(), 1U);
}

// Tests that the `RanLocalSaveFallback` metric records that an existing local
// IBAN was not saved during the local IBAN save fallback for a server upload
// failure.
TEST_F(IbanSaveManagerTest,
       Metrics_OnUploadIban_FallbackToLocalSave_LocalIbanNotAdded) {
  base::HistogramTester histogram_tester;
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  personal_data().payments_data_manager().AddAsLocalIban(iban);
  ASSERT_EQ(personal_data().payments_data_manager().GetLocalIbans().size(), 1U);
  GetIbanSaveManager().OnDidUploadIbanForTesting(
      iban, /*show_save_prompt=*/true,
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure);

  histogram_tester.ExpectUniqueSample("Autofill.IbanUpload.SaveFailed", false,
                                      1);
  EXPECT_EQ(personal_data().payments_data_manager().GetLocalIbans().size(), 1U);
}

// Tests that the `RanLocalSaveFallback` metric records that a matched local
// IBAN (same IBAN value but different nickname) was not saved during the local
// IBAN save fallback for a server upload failure.
TEST_F(
    IbanSaveManagerTest,
    Metrics_OnUploadIban_FallbackToLocalSave_LocalIbanWithDifferentNicknameNotAdded) {
  base::HistogramTester histogram_tester;
  Iban iban;
  iban.set_value(std::u16string(test::kIbanValue16));
  personal_data().payments_data_manager().AddAsLocalIban(iban);
  ASSERT_EQ(personal_data().payments_data_manager().GetLocalIbans().size(), 1U);
  iban.set_nickname(u"new nickname");
  GetIbanSaveManager().OnDidUploadIbanForTesting(
      iban, /*show_save_prompt=*/true,
      payments::PaymentsAutofillClient::PaymentsRpcResult::kPermanentFailure);

  histogram_tester.ExpectUniqueSample("Autofill.IbanUpload.SaveFailed", false,
                                      1);
  EXPECT_EQ(personal_data().payments_data_manager().GetLocalIbans().size(), 1U);
  EXPECT_EQ(
      personal_data().payments_data_manager().GetLocalIbans()[0]->nickname(),
      u"");
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace
}  // namespace autofill
