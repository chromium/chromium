// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_access_manager.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/mock_test_payments_network_interface.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

constexpr char16_t kFullIbanValue[] = u"CH5604835012345678009";
constexpr int64_t kInstrumentId = 12345678;
constexpr int kDaysSinceLastUsed = 3;
constexpr int kDefaultUnmaskIbanLatencyMs = 200;
constexpr size_t kDefaultUseCount = 4;

}  // namespace

class IbanAccessManagerTest : public testing::Test {
 public:
  IbanAccessManagerTest() {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    autofill_client_.set_personal_data_manager(
        std::make_unique<TestPersonalDataManager>());
    autofill_client_.set_sync_service(&sync_service_);
    autofill_client_.set_test_payments_network_interface(
        std::make_unique<MockTestPaymentsNetworkInterface>());
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
    iban_access_manager_ =
        std::make_unique<IbanAccessManager>(&autofill_client_);
  }

  void SetUpUnmaskIbanCall(bool is_successful,
                           const std::u16string& value,
                           int latency_ms = 0) {
    ON_CALL(*payments_network_interface(), UnmaskIban)
        .WillByDefault(
            [=, this](
                const payments::PaymentsNetworkInterface::
                    UnmaskIbanRequestDetails&,
                base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                                        const std::u16string&)> callback) {
              task_environment_.FastForwardBy(base::Milliseconds(latency_ms));
              std::move(callback).Run(
                  is_successful
                      ? AutofillClient::PaymentsRpcResult::kSuccess
                      : AutofillClient::PaymentsRpcResult::kPermanentFailure,
                  value);
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

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  syncer::TestSyncService sync_service_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<IbanAccessManager> iban_access_manager_;
};

// Verify that `FetchValue` returns the correct value for an existing local
// IBAN.
TEST_F(IbanAccessManagerTest, FetchValue_ExistingLocalIban) {
  Suggestion suggestion(PopupItemId::kIbanEntry);
  Iban local_iban = test::GetLocalIban();
  local_iban.set_value(kFullIbanValue);
  personal_data().AddIbanForTest(std::make_unique<Iban>(local_iban));
  suggestion.payload =
      Suggestion::BackendId(Suggestion::Guid(local_iban.guid()));

  base::MockCallback<IbanAccessManager::OnIbanFetchedCallback> callback;
  EXPECT_CALL(callback, Run(std::u16string(kFullIbanValue)));
  iban_access_manager_->FetchValue(suggestion, callback.Get());
}

// Verify that `FetchValue` does not trigger callback if local IBAN does not
// exist.
TEST_F(IbanAccessManagerTest, FetchValue_NonExistingLocalIban) {
  Suggestion suggestion(PopupItemId::kIbanEntry);
  Iban local_iban;
  suggestion.payload =
      Suggestion::BackendId(Suggestion::Guid(local_iban.guid()));

  base::MockCallback<IbanAccessManager::OnIbanFetchedCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);
  iban_access_manager_->FetchValue(suggestion, callback.Get());
}

// Verify that an UnmaskIban call won't be triggered if no server IBAN with the
// same `instrument_id` as BackendId is found.
TEST_F(IbanAccessManagerTest, NoServerIbanWithBackendId_DoesNotUnmask) {
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::InstrumentId(kInstrumentId);

  // Add a server IBAN with a different instrument_id and verify `FetchValue`
  // is not triggered.
  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(12345679));
  personal_data().AddServerIban(server_iban);

  EXPECT_CALL(*payments_network_interface(), UnmaskIban).Times(0);
  base::MockCallback<IbanAccessManager::OnIbanFetchedCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);
  iban_access_manager_->FetchValue(suggestion, callback.Get());
}

// Verify that a successful `UnmaskIban` call results in the `FetchValue`
// returning the complete server IBAN value.
TEST_F(IbanAccessManagerTest, ServerIban_BackendId_Success) {
  SetUpUnmaskIbanCall(/*is_successful=*/true, /*value=*/kFullIbanValue);

  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(kInstrumentId));
  personal_data().AddServerIban(server_iban);
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::InstrumentId(kInstrumentId);

  base::MockCallback<IbanAccessManager::OnIbanFetchedCallback> callback;
  EXPECT_CALL(callback, Run(std::u16string(kFullIbanValue)));
  iban_access_manager_->FetchValue(suggestion, callback.Get());
}

// Verify that a failed `UnmaskIban` call results in the method `OnIbanFetched`
// not being called.
TEST_F(IbanAccessManagerTest, ServerIban_BackendId_Failure) {
  SetUpUnmaskIbanCall(/*is_successful=*/false, /*value=*/kFullIbanValue);

  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(kInstrumentId));
  personal_data().AddServerIban(server_iban);
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::InstrumentId(kInstrumentId);

  base::MockCallback<IbanAccessManager::OnIbanFetchedCallback> callback;
  EXPECT_CALL(callback, Run).Times(0);
  iban_access_manager_->FetchValue(suggestion, callback.Get());

  EXPECT_CALL(*payments_network_interface(), UnmaskIban).Times(0);
}

// Verify that there will be no progress dialog when unmasking a local IBAN.
TEST_F(IbanAccessManagerTest, FetchValue_LocalIbanNoProgressDialog) {
  Suggestion suggestion(PopupItemId::kIbanEntry);
  Iban local_iban = test::GetLocalIban();
  local_iban.set_value(kFullIbanValue);
  personal_data().AddIbanForTest(std::make_unique<Iban>(local_iban));
  suggestion.payload =
      Suggestion::BackendId(Suggestion::Guid(local_iban.guid()));

  base::MockCallback<IbanAccessManager::OnIbanFetchedCallback> callback;
  iban_access_manager_->FetchValue(suggestion, callback.Get());

  EXPECT_FALSE(autofill_client_.autofill_progress_dialog_shown());
}

// Verify that there will be a progress dialog when unmasking a server IBAN.
TEST_F(IbanAccessManagerTest, FetchValue_ServerIban_ProgressDialog_Success) {
  SetUpUnmaskIbanCall(/*is_successful=*/true, /*value=*/kFullIbanValue);

  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(kInstrumentId));
  personal_data().AddServerIban(server_iban);
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::InstrumentId(kInstrumentId);

  base::MockCallback<IbanAccessManager::OnIbanFetchedCallback> callback;
  iban_access_manager_->FetchValue(suggestion, callback.Get());

  EXPECT_TRUE(autofill_client_.autofill_progress_dialog_shown());
  EXPECT_FALSE(autofill_client_.autofill_error_dialog_shown());
}

// Verify that there will be a progress dialog when unmasking a server IBAN,
// followed by an error dialog if it fails to be unmasked.
TEST_F(IbanAccessManagerTest, FetchValue_ServerIban_ProgressDialog_Failure) {
  SetUpUnmaskIbanCall(/*is_successful=*/false, /*value=*/kFullIbanValue);

  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(kInstrumentId));
  personal_data().AddServerIban(server_iban);
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::InstrumentId(kInstrumentId);

  base::MockCallback<IbanAccessManager::OnIbanFetchedCallback> callback;
  iban_access_manager_->FetchValue(suggestion, callback.Get());

  EXPECT_TRUE(autofill_client_.autofill_progress_dialog_shown());
  EXPECT_TRUE(autofill_client_.autofill_error_dialog_shown());
}

// Verify that local IBAN metadata has been recorded correctly.
TEST_F(IbanAccessManagerTest, LocalIban_LogUsageMetric) {
  base::HistogramTester histogram_tester;
  Suggestion suggestion(PopupItemId::kIbanEntry);
  Iban local_iban = test::GetLocalIban();
  local_iban.set_value(kFullIbanValue);
  local_iban.set_use_count(kDefaultUseCount);
  personal_data().AddIbanForTest(std::make_unique<Iban>(local_iban));
  suggestion.payload =
      Suggestion::BackendId(Suggestion::Guid(local_iban.guid()));

  task_environment_.FastForwardBy(base::Days(kDaysSinceLastUsed));
  base::MockCallback<IbanAccessManager::OnIbanFetchedCallback> callback;
  iban_access_manager_->FetchValue(suggestion, callback.Get());

  histogram_tester.ExpectUniqueSample(
      "Autofill.DaysSinceLastUse.StoredIban.Local", kDaysSinceLastUsed, 1);
  EXPECT_EQ(personal_data().GetIbanByGUID(local_iban.guid())->use_count(),
            kDefaultUseCount + 1);
}

// Verify that server IBAN metadata has been recorded correctly.
TEST_F(IbanAccessManagerTest, ServerIban_LogUsageMetric) {
  base::HistogramTester histogram_tester;
  SetUpUnmaskIbanCall(/*is_successful=*/true, /*value=*/kFullIbanValue);

  Iban server_iban = test::GetServerIban();
  server_iban.set_use_count(kDefaultUseCount);
  server_iban.set_identifier(Iban::InstrumentId(kInstrumentId));
  personal_data().AddServerIban(server_iban);
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::InstrumentId(kInstrumentId);

  task_environment_.FastForwardBy(base::Days(kDaysSinceLastUsed));
  base::MockCallback<IbanAccessManager::OnIbanFetchedCallback> callback;
  iban_access_manager_->FetchValue(suggestion, callback.Get());

  histogram_tester.ExpectUniqueSample(
      "Autofill.DaysSinceLastUse.StoredIban.Server", kDaysSinceLastUsed, 1);
  EXPECT_EQ(personal_data()
                .GetIbanByInstrumentId(server_iban.instrument_id())
                ->use_count(),
            kDefaultUseCount + 1);
}

// Verify that the duration of successful `UnmaskIban` call is logged correctly.
TEST_F(IbanAccessManagerTest, UnmaskServerIban_Success_Metric) {
  base::HistogramTester histogram_tester;
  SetUpUnmaskIbanCall(/*is_successful=*/true, /*value=*/kFullIbanValue,
                      /*latency_ms=*/kDefaultUnmaskIbanLatencyMs);

  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(kInstrumentId));
  personal_data().AddServerIban(server_iban);
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::InstrumentId(kInstrumentId);

  base::MockCallback<IbanAccessManager::OnIbanFetchedCallback> callback;
  iban_access_manager_->FetchValue(suggestion, callback.Get());

  histogram_tester.ExpectUniqueSample(
      "Autofill.Iban.UnmaskIbanDuration.Success", kDefaultUnmaskIbanLatencyMs,
      1);
  histogram_tester.ExpectUniqueSample("Autofill.Iban.UnmaskIbanDuration",
                                      kDefaultUnmaskIbanLatencyMs, 1);
}

// Verify that duration of failed `UnmaskIban` call is logged correctly.
TEST_F(IbanAccessManagerTest, UnmaskServerIban_Failure_Metric) {
  base::HistogramTester histogram_tester;
  SetUpUnmaskIbanCall(/*is_successful=*/false, /*value=*/kFullIbanValue,
                      /*latency_ms=*/kDefaultUnmaskIbanLatencyMs);

  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(kInstrumentId));
  personal_data().AddServerIban(server_iban);
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::InstrumentId(kInstrumentId);

  iban_access_manager_->FetchValue(suggestion, base::DoNothing());

  histogram_tester.ExpectUniqueSample(
      "Autofill.Iban.UnmaskIbanDuration.Failure", kDefaultUnmaskIbanLatencyMs,
      1);
  histogram_tester.ExpectUniqueSample("Autofill.Iban.UnmaskIbanDuration",
                                      kDefaultUnmaskIbanLatencyMs, 1);
}

// Verify that UnmaskIbanResult records true for a successful call.
TEST_F(IbanAccessManagerTest, UnmaskIbanResult_Metric_Success) {
  base::HistogramTester histogram_tester;
  SetUpUnmaskIbanCall(/*is_successful=*/true, /*value=*/kFullIbanValue);

  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(kInstrumentId));
  personal_data().AddServerIban(server_iban);
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::InstrumentId(kInstrumentId);

  iban_access_manager_->FetchValue(suggestion, base::DoNothing());

  histogram_tester.ExpectUniqueSample("Autofill.Iban.UnmaskIbanResult", true,
                                      1);
}

// Verify that UnmaskIbanResult records false for a failed call.
TEST_F(IbanAccessManagerTest, UnmaskIbanResult_Metric_Failure) {
  base::HistogramTester histogram_tester;
  SetUpUnmaskIbanCall(/*is_successful=*/false, /*value=*/u"");

  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(kInstrumentId));
  personal_data().AddServerIban(server_iban);
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::InstrumentId(kInstrumentId);

  iban_access_manager_->FetchValue(suggestion, base::DoNothing());

  histogram_tester.ExpectUniqueSample("Autofill.Iban.UnmaskIbanResult", false,
                                      1);
}

}  // namespace autofill
