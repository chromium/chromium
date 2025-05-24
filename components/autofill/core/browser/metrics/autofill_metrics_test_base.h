// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_TEST_BASE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_TEST_BASE_H_

#include "base/check_deref.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/touch_to_fill/touch_to_fill_delegate.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {

constexpr char kTestProfileId[] = "00000000-0000-0000-0000-000000000001";
constexpr char kTestProfile2Id[] = "00000000-0000-0000-0000-000000000002";
constexpr char kTestLocalCardId[] = "10000000-0000-0000-0000-000000000001";
constexpr char kTestMaskedCardId[] = "10000000-0000-0000-0000-000000000002";
// These variables store the GUIDs of a Local and a masked Server card which
// have the same card attributes, i.e., are duplicates of each other.
constexpr char kTestDuplicateLocalCardId[] =
    "10000000-0000-0000-0000-000000000004";
constexpr char kTestDuplicateMaskedCardId[] =
    "10000000-0000-0000-0000-000000000005";

class MockPaymentsAutofillClient : public payments::TestPaymentsAutofillClient {
 public:
  explicit MockPaymentsAutofillClient(AutofillClient* client);
  ~MockPaymentsAutofillClient() override;

  MOCK_METHOD(bool,
              ShowTouchToFillCreditCard,
              ((base::WeakPtr<TouchToFillDelegate>),
               (base::span<const autofill::Suggestion>)),
              (override));
};

class MockCreditCardAccessManager : public CreditCardAccessManager {
 public:
  explicit MockCreditCardAccessManager(BrowserAutofillManager* bam);
  ~MockCreditCardAccessManager() override;
  MOCK_METHOD(void,
              FetchCreditCard,
              (const CreditCard* card,
               OnCreditCardFetchedCallback on_credit_card_fetched),
              (override));
};

class TestBrowserAutofillManager : public autofill::TestBrowserAutofillManager {
 public:
  explicit TestBrowserAutofillManager(AutofillDriver* driver);
  void Reset() override;
};

class AutofillMetricsBaseTest {
 public:
  AutofillMetricsBaseTest();
  virtual ~AutofillMetricsBaseTest();

 protected:
  void SetUpHelper();

  void TearDownHelper();

  void CreateAmbiguousProfiles();

  // Removes all existing profiles and creates one profile.
  void RecreateProfile();

  // Removes all existing credit cards and then invokes CreateCreditCards to
  // create the cards. `include_cvc_in_cards` will set a CVC value on each card
  // created for non-iOS platforms.
  // TODO(crbug.com/40770602): Migrate this to a params builder pattern or
  // something.
  void RecreateCreditCards(bool include_local_credit_card,
                           bool include_masked_server_credit_card,
                           bool masked_card_is_enrolled_for_virtual_card,
                           bool include_cvc_in_cards = true);

  // Creates a local, masked server, and/or virtual credit card, according to
  // the parameters. `include_cvc_in_cards` will set a CVC value on each card
  // created for non-iOS platforms.
  void CreateCreditCards(bool include_local_credit_card,
                         bool include_masked_server_credit_card,
                         bool masked_card_is_enrolled_for_virtual_card,
                         bool include_cvc_in_cards = true);

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
  void OnDidGetRealPan(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
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
  void SimulateUserChangedField(FormData& form,
                                const FormFieldData& field,
                                base::TimeTicks timestamp = {}) {
    SimulateUserChangedFieldTo(form, field.global_id(),
                               field.value() + u"_changed", timestamp);
  }

  // TODO(crbug.com/40100455): Remove this overload.
  void SimulateUserChangedFieldTo(FormData& form,
                                  const FormFieldData& field,
                                  const std::u16string& new_value,
                                  base::TimeTicks timestamp = {}) {
    SimulateUserChangedFieldTo(form, field.global_id(), new_value, timestamp);
  }

  // Emulates that the user manually changed a field by resetting the
  // `is_autofilled` field attribute, settings the field's value to `new_value`
  // and notifying the `AutofillManager` of the change that is emulated to have
  // happened at `timestamp`.
  void SimulateUserChangedFieldTo(FormData& form,
                                  const FieldGlobalId& field_id,
                                  const std::u16string& new_value,
                                  base::TimeTicks timestamp = {}) {
    // TODO(crbug.com/40100455): Remove const_cast.
    FormFieldData& field = const_cast<FormFieldData&>(
        CHECK_DEREF(form.FindFieldByGlobalId(field_id)));
    // Assert that the field is actually set to a different value.
    ASSERT_NE(field.value(), new_value);
    field.set_is_autofilled(false);
    field.set_value(new_value);
    if (field.IsSelectElement()) {
      autofill_manager().OnSelectControlSelectionChanged(form,
                                                         field.global_id());
    } else {
      autofill_manager().OnTextFieldValueChanged(form, field.global_id(),
                                                 timestamp);
    }
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
        form, mojom::SubmissionSource::FORM_SUBMISSION);
  }

  static CreditCard BuildCard(const std::u16string& real_pan,
                              bool is_virtual_card = false);

  FormData GetAndAddSeenForm(const test::FormDescription& form_description) {
    FormData form = test::GetFormData(form_description);
    autofill_driver().SetLocalFrameToken(form.host_frame());
    autofill_manager().AddSeenForm(form,
                                   test::GetHeuristicTypes(form_description),
                                   test::GetServerTypes(form_description));
    return form;
  }

  void DidShowAutofillSuggestions(
      const FormData& form,
      size_t field_index = 0,
      SuggestionType suggestion_type = SuggestionType::kAddressEntry) {
    Suggestion suggestion(suggestion_type);
    autofill_manager().DidShowSuggestions(
        {suggestion}, form, form.fields()[field_index].global_id(), {});
  }

  void FillTestProfile(const FormData& form, size_t field_index = 0) {
    FillProfileByGUID(form, kTestProfileId, field_index);
  }

  void FillProfileByGUID(const FormData& form,
                         const std::string& profile_guid,
                         size_t field_index = 0) {
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form,
        form.fields()[field_index].global_id(),
        personal_data().address_data_manager().GetProfileByGUID(profile_guid),
        AutofillTriggerSource::kPopup);
  }

  void UndoAutofill(const FormData& form) {
    autofill_manager().UndoAutofill(mojom::ActionPersistence::kFill, form,
                                    form.fields().front());
  }

  [[nodiscard]] FormData CreateEmptyForm() {
    FormData form;
    form.set_host_frame(autofill_driver_->GetFrameToken());
    form.set_renderer_id(test::MakeFormRendererId());
    form.set_name(u"TestForm");
    form.set_url(GURL("https://example.com/form.html"));
    form.set_action(GURL("https://example.com/submit.html"));
    form.set_main_frame_origin(url::Origin::Create(autofill_driver_->url()));
    return form;
  }

  [[nodiscard]] FormData CreateForm(std::vector<FormFieldData> fields) {
    FormData form = CreateEmptyForm();
    form.set_fields(std::move(fields));
    return form;
  }

  TestAutofillClient& autofill_client() { return *autofill_client_; }

  TestAutofillDriver& autofill_driver() { return *autofill_driver_; }

  TestBrowserAutofillManager& autofill_manager() {
    return static_cast<TestBrowserAutofillManager&>(
        autofill_driver_->GetAutofillManager());
  }

  AutofillExternalDelegate& external_delegate() {
    return *test_api(autofill_manager()).external_delegate();
  }

  MockCreditCardAccessManager& credit_card_access_manager() {
    return static_cast<MockCreditCardAccessManager&>(
        autofill_manager().GetCreditCardAccessManager());
  }

  TestPersonalDataManager& personal_data() {
    return autofill_client_->GetPersonalDataManager();
  }

  ukm::TestUkmRecorder& test_ukm_recorder() {
    return *autofill_client_->GetUkmRecorder();
  }

  MockPaymentsAutofillClient& payments_autofill_client() {
    return static_cast<MockPaymentsAutofillClient&>(
        *autofill_client_->GetPaymentsAutofillClient());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  syncer::TestSyncService sync_service_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;

 private:
  void CreateTestAutofillProfiles();
};

}  // namespace autofill::autofill_metrics

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_METRICS_AUTOFILL_METRICS_TEST_BASE_H_
