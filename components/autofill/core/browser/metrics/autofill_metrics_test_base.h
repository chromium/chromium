// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_TEST_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_TEST_BASE_H_

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/sync/driver/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::metrics {

constexpr char kTestGuid[] = "00000000-0000-0000-0000-000000000001";

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient();
  ~MockAutofillClient() override;
  MOCK_METHOD(void, ExecuteCommand, (int), (override));
};

class AutofillMetricsBaseTest : public testing::Test {
 public:
  explicit AutofillMetricsBaseTest(bool is_in_any_main_frame = true);
  ~AutofillMetricsBaseTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  void CreateAmbiguousProfiles();

  // Removes all existing profiles and creates one profile.
  // |is_server| allows creation of |SERVER_PROFILE|.
  void RecreateProfile(bool is_server);

  // Removes all existing credit cards and creates a local, masked server,
  // full server, and/or virtual credit card, according to the parameters.
  // TODO(crbug/1216615): Migrate this to a params builder pattern or
  // something.
  void RecreateCreditCards(bool include_local_credit_card,
                           bool include_masked_server_credit_card,
                           bool include_full_server_credit_card,
                           bool masked_card_is_enrolled_for_virtual_card);

  // Creates a local card to existing card deck or clear them all and then add a
  // new local card.
  // The GUID for the card created is returned as a string.
  std::string CreateLocalMasterCard(bool clear_existing_cards = false);

  // Creates a local card and then a duplicate server card with the same
  // credentials/info.
  // The GUIDs for the cards crated are returned as a vector of strings.
  std::vector<std::string> CreateLocalAndDuplicateServerCreditCard();

  void AddMaskedServerCreditCardWithOffer(std::string guid,
                                          std::string offer_reward_amount,
                                          GURL url,
                                          int64_t id,
                                          bool offer_expired = false);

  // If set to true, then user is capable of using FIDO authentication for
  // card unmasking.
  void SetFidoEligibility(bool is_verifiable);

  // Mocks a RPC response from Payments.
  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan,
                       bool is_virtual_card = false);

  // Mocks a RPC response from Payments, but where a non-HTTP_OK response
  // stopped it from parsing a valid response.
  void OnDidGetRealPanWithNonHttpOkResponse();

  // Purge recorded UKM metrics for running more tests.
  void PurgeUKM();

  void ResetDriverToCommitMetrics() { autofill_driver_.reset(); }

  void ChangeTextField(const FormData& form,
                       const FormFieldData& field,
                       base::TimeTicks timestamp = {}) {
    autofill_manager().OnTextFieldDidChange(form, field, gfx::RectF(),
                                            timestamp);
  }

  void FillAutofillFormData(const FormData& form,
                            base::TimeTicks timestamp = {}) {
    autofill_manager().OnDidFillAutofillFormData(form, timestamp);
  }

  void SeeForm(const FormData& form) {
    autofill_manager().OnFormsSeen({form}, {});
  }

  void SubmitForm(const FormData& form) {
    autofill_manager().OnFormSubmitted(
        form, /*known_success=*/false,
        mojom::SubmissionSource::FORM_SUBMISSION);
  }

  // Mocks a credit card fetching was completed. This mock starts from the
  // BrowserAutofillManager. Use these if your test does not depends on
  // OnDidGetRealPan but just need to mock the card fetching result (so that
  // you don't need to branch on what auth method was used).
  void OnCreditCardFetchingSuccessful(const std::u16string& real_pan,
                                      bool is_virtual_card = false);
  void OnCreditCardFetchingFailed();

  TestBrowserAutofillManager& autofill_manager() {
    return static_cast<TestBrowserAutofillManager&>(
        *autofill_driver_->autofill_manager());
  }

  TestPersonalDataManager& personal_data() {
    return *autofill_client_->GetPersonalDataManager();
  }

  const bool is_in_any_main_frame_ = true;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockAutofillClient> autofill_client_;
  raw_ptr<ukm::TestUkmRecorder> test_ukm_recorder_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  raw_ptr<AutofillExternalDelegate> external_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  void CreateTestAutofillProfiles();

  CreditCard credit_card_ = test::GetMaskedServerCard();
};

}  // namespace autofill::metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_TEST_BASE_H_
