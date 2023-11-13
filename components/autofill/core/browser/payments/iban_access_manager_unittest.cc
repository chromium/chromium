// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/iban_access_manager.h"

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

}  // namespace

class TestAccessor : public IbanAccessManager::Accessor {
 public:
  void OnIbanFetched(const std::u16string& value) override {
    fetched_iban_ = value;
  }

  std::optional<std::u16string> fetched_iban() const { return fetched_iban_; }

  base::WeakPtr<TestAccessor> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // The full IBAN returned from OnIbanFetched().
  std::optional<std::u16string> fetched_iban_;
  base::WeakPtrFactory<TestAccessor> weak_ptr_factory_{this};
};

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
    accessor_ = std::make_unique<TestAccessor>();
    iban_access_manager_ =
        std::make_unique<IbanAccessManager>(&autofill_client_);
  }

  std::optional<std::u16string> GetFetchedIban() {
    return accessor_->fetched_iban();
  }

  void SetUpUnmaskIbanCall(bool is_successful, const std::u16string& value) {
    ON_CALL(*payments_network_interface(), UnmaskIban)
        .WillByDefault(
            [is_successful, value](
                const payments::PaymentsNetworkInterface::
                    UnmaskIbanRequestDetails&,
                base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                                        const std::u16string&)> callback) {
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

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  syncer::TestSyncService sync_service_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAccessor> accessor_;
  std::unique_ptr<IbanAccessManager> iban_access_manager_;
};

// Verify that `FetchValue` returns the correct value for a `ValueToFill`
// payload.
TEST_F(IbanAccessManagerTest, FetchValue_WithValueToFill) {
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::ValueToFill(kFullIbanValue);

  iban_access_manager_->FetchValue(suggestion, accessor_->GetWeakPtr());

  EXPECT_EQ(kFullIbanValue, GetFetchedIban().value());
}

// Verify that `FetchValue` returns empty value if `ValueToFill` is empty.
TEST_F(IbanAccessManagerTest, FetchValue_WithValueToFill_EmptyValue) {
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::ValueToFill(u"");

  iban_access_manager_->FetchValue(suggestion, accessor_->GetWeakPtr());

  EXPECT_FALSE(GetFetchedIban());
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
  iban_access_manager_->FetchValue(suggestion, accessor_->GetWeakPtr());

  EXPECT_FALSE(GetFetchedIban());
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

  iban_access_manager_->FetchValue(suggestion, accessor_->GetWeakPtr());

  EXPECT_EQ(kFullIbanValue, GetFetchedIban().value());
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

  iban_access_manager_->FetchValue(suggestion, accessor_->GetWeakPtr());

  EXPECT_CALL(*payments_network_interface(), UnmaskIban).Times(0);
  EXPECT_FALSE(GetFetchedIban());
}

// Verify that a failed `UnmaskIban` call results in the method `OnIbanFetched`
// not being called.
TEST_F(IbanAccessManagerTest, ServerIban_BackendId_SuccessButEmptyValue) {
  SetUpUnmaskIbanCall(/*is_successful=*/true, /*value=*/u"");

  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(kInstrumentId));
  personal_data().AddServerIban(server_iban);
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::InstrumentId(kInstrumentId);

  iban_access_manager_->FetchValue(suggestion, accessor_->GetWeakPtr());

  EXPECT_CALL(*payments_network_interface(), UnmaskIban).Times(0);
  EXPECT_FALSE(GetFetchedIban());
}

// Verify that a nullptr accessor results in the `FetchValue` not
// triggering an `UnmaskIban` request.
TEST_F(IbanAccessManagerTest, NoAccessor_NotTriggerUnmaskIbanRequest) {
  SetUpUnmaskIbanCall(/*is_successful=*/true, /*value=*/kFullIbanValue);

  Iban server_iban = test::GetServerIban();
  server_iban.set_identifier(Iban::InstrumentId(kInstrumentId));
  personal_data().AddServerIban(server_iban);
  Suggestion suggestion(PopupItemId::kIbanEntry);
  suggestion.payload = Suggestion::InstrumentId(kInstrumentId);

  iban_access_manager_->FetchValue(suggestion, /*accessor=*/nullptr);

  EXPECT_CALL(*payments_network_interface(), UnmaskIban).Times(0);
  EXPECT_FALSE(GetFetchedIban());
}

}  // namespace autofill
