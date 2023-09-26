// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_TEST_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_TEST_BASE_H_

#include "base/metrics/metrics_hashes.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

constexpr char kTestProfileId[] = "00000000-0000-0000-0000-000000000001";
constexpr char kTestLocalCardId[] = "10000000-0000-0000-0000-000000000001";
constexpr char kTestMaskedCardId[] = "10000000-0000-0000-0000-000000000002";
constexpr char kTestFullServerCardId[] = "10000000-0000-0000-0000-000000000003";
// These variables store the GUIDs of a Local and a masked Server card which
// have the same card attributes, i.e., are duplicates of each other.
constexpr char kTestDuplicateLocalCardId[] =
    "10000000-0000-0000-0000-000000000004";
constexpr char kTestDuplicateMaskedCardId[] =
    "10000000-0000-0000-0000-000000000005";

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient();
  ~MockAutofillClient() override;
  MOCK_METHOD(bool, IsTouchToFillCreditCardSupported, (), (override));
  MOCK_METHOD(bool,
              ShowTouchToFillCreditCard,
              (base::WeakPtr<TouchToFillDelegate>,
               base::span<const autofill::CreditCard>),
              (override));
};

class AutofillMetricsBaseTest {
 public:
  explicit AutofillMetricsBaseTest(bool is_in_any_main_frame = true);
  virtual ~AutofillMetricsBaseTest();

 protected:
  void SetUpHelper();

  void TearDownHelper();

  void CreateAmbiguousProfiles();

  // Removes all existing profiles and creates one profile.
  // |is_server| allows creation of |SERVER_PROFILE|.
  void RecreateProfile(bool is_server);

  // Removes all existing credit cards and then invokes CreateCreditCards to
  // create the cards.
  // TODO(crbug/1216615): Migrate this to a params builder pattern or
  // something.
  void RecreateCreditCards(bool include_local_credit_card,
                           bool include_masked_server_credit_card,
                           bool include_full_server_credit_card,
                           bool masked_card_is_enrolled_for_virtual_card);

  // Creates a local, masked server, full server, and/or virtual credit card,
  // according to the parameters.
  void CreateCreditCards(bool include_local_credit_card,
                         bool include_masked_server_credit_card,
                         bool include_full_server_credit_card,
                         bool masked_card_is_enrolled_for_virtual_card);

  // Creates a local card and then a duplicate server card with the same
  // credentials/info.
  void CreateLocalAndDuplicateServerCreditCard();

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

  // Convenience wrapper for `EmulateUserChangedTextFieldTo` that appends
  // '_changed' to the fields value.
  void SimulateUserChangedTextField(const FormData& form,
                                    FormFieldData& field,
                                    base::TimeTicks timestamp = {}) {
    SimulateUserChangedTextFieldTo(form, field, field.value + u"_changed",
                                   timestamp);
  }

  // Emulates that the user manually changed a field by resetting the
  // `is_autofilled` field attribute, settings the field's value to `new_value`
  // and notifying the `AutofillManager` of the change that is emulated to have
  // happened at `timestamp`.
  void SimulateUserChangedTextFieldTo(const FormData& form,
                                      FormFieldData& field,
                                      const std::u16string& new_value,
                                      base::TimeTicks timestamp = {}) {
    // Assert that the field is actually set to a different value.
    ASSERT_NE(field.value, new_value);
    field.is_autofilled = false;
    field.value = new_value;
    autofill_manager().OnTextFieldDidChange(form, field, gfx::RectF(),
                                            timestamp);
  }

  // TODO(crbug.com/1368096): Remove this method once the metrics are fixed.
  void SimulateUserChangedTextFieldWithoutActuallyChangingTheValue(
      const FormData& form,
      FormFieldData& field,
      base::TimeTicks timestamp = {}) {
    field.is_autofilled = false;
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

  FormData GetAndAddSeenForm(const test::FormDescription& form_description) {
    FormData form = test::GetFormData(form_description);
    autofill_manager().AddSeenForm(form,
                                   test::GetHeuristicTypes(form_description),
                                   test::GetServerTypes(form_description));
    return form;
  }

  void DidShowAutofillSuggestions(const FormData& form,
                                  size_t field_index = 0) {
    autofill_manager().DidShowSuggestions(
        /*has_autofill_suggestions=*/true, form, form.fields[field_index]);
  }

  void FillTestProfile(const FormData& form) {
    autofill_manager().FillOrPreviewForm(
        mojom::AutofillActionPersistence::kFill, form, form.fields.front(),
        Suggestion::BackendId(kTestProfileId),
        {.trigger_source = AutofillTriggerSource::kPopup});
  }

  [[nodiscard]] FormData CreateEmptyForm() {
    FormData form;
    form.host_frame = test::MakeLocalFrameToken();
    form.unique_renderer_id = test::MakeFormRendererId();
    form.name = u"TestForm";
    form.url = GURL("https://example.com/form.html");
    form.action = GURL("https://example.com/submit.html");
    form.main_frame_origin =
        url::Origin::Create(autofill_client_->form_origin());
    return form;
  }

  [[nodiscard]] FormData CreateForm(std::vector<FormFieldData> fields) {
    FormData form = CreateEmptyForm();
    form.fields = std::move(fields);
    return form;
  }

  TestBrowserAutofillManager& autofill_manager() {
    return static_cast<TestBrowserAutofillManager&>(
        autofill_driver_->GetAutofillManager());
  }

  AutofillExternalDelegate& external_delegate() {
    return *test_api(autofill_manager()).external_delegate();
  }

  TestPersonalDataManager& personal_data() {
    return *autofill_client_->GetPersonalDataManager();
  }

  ukm::TestUkmRecorder& test_ukm_recorder() {
    return *autofill_client_->GetTestUkmRecorder();
  }

  const bool is_in_any_main_frame_ = true;
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<MockAutofillClient> autofill_client_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;

 private:
  void CreateTestAutofillProfiles();

  base::test::ScopedFeatureList scoped_feature_list_async_parse_form_;
  CreditCard credit_card_ = test::WithCvc(test::GetMaskedServerCard());
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_TEST_BASE_H_
