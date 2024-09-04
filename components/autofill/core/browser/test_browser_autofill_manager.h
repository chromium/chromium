// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_BROWSER_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_BROWSER_AUTOFILL_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_form_filler.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {

class AutofillDriver;
class FormStructure;
class TestAutofillClient;
class TestPersonalDataManager;

class TestBrowserAutofillManager : public BrowserAutofillManager {
 public:
  explicit TestBrowserAutofillManager(AutofillDriver* driver);

  TestBrowserAutofillManager(const TestBrowserAutofillManager&) = delete;
  TestBrowserAutofillManager& operator=(const TestBrowserAutofillManager&) =
      delete;

  ~TestBrowserAutofillManager() override;

  // AutofillManager overrides.
  // The overrides ensure that the thread is blocked until the form has been
  // parsed.
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;
  void OnFormsSeen(const std::vector<FormData>& updated_forms,
                   const std::vector<FormGlobalId>& removed_forms) override;
  void OnCaretMovedInFormField(const FormData& form,
                               const FieldGlobalId& field_id,
                               const gfx::Rect& caret_bounds) override;
  void OnTextFieldDidChange(const FormData& form,
                            const FieldGlobalId& field_id,
                            const base::TimeTicks timestamp) override;
  void OnTextFieldDidScroll(const FormData& form,
                            const FieldGlobalId& field_id) override;
  void OnSelectControlDidChange(const FormData& form,
                                const FieldGlobalId& field_id) override;
  void OnAskForValuesToFill(
      const FormData& form,
      const FieldGlobalId& field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source) override;
  void OnFocusOnFormField(const FormData& form,
                          const FieldGlobalId& field_id) override;
  void OnDidFillAutofillFormData(const FormData& form,
                                 const base::TimeTicks timestamp) override;
  void OnJavaScriptChangedAutofilledValue(const FormData& form,
                                          const FieldGlobalId& field_id,
                                          const std::u16string& old_value,
                                          bool formatting_only) override;
  void OnFormSubmitted(const FormData& form,
                       const bool known_success,
                       const mojom::SubmissionSource source) override;

  // BrowserAutofillManager overrides.
  bool IsAutofillProfileEnabled() const override;
  bool IsAutofillPaymentMethodsEnabled() const override;
  void StoreUploadVotesAndLogQualityCallback(
      FormSignature form_signature,
      base::OnceClosure callback) override;
  void UploadVotesAndLogQuality(std::unique_ptr<FormStructure> submitted_form,
                                base::TimeTicks interaction_time,
                                base::TimeTicks submission_time,
                                bool observed_submission,
                                const ukm::SourceId source_id) override;
  const gfx::Image& GetCardImage(const CreditCard& credit_card) override;
  bool MaybeStartVoteUploadProcess(
      std::unique_ptr<FormStructure> form_structure,
      bool observed_submission) override;

  // Unique to TestBrowserAutofillManager:

  void AddSeenForm(const FormData& form,
                   const std::vector<FieldType>& field_types,
                   bool preserve_values_in_form_structure = false) {
    AddSeenForm(form, /*heuristic_types=*/field_types,
                /*server_types=*/field_types,
                preserve_values_in_form_structure);
  }

  void AddSeenForm(const FormData& form,
                   const std::vector<FieldType>& heuristic_types,
                   const std::vector<FieldType>& server_types,
                   bool preserve_values_in_form_structure = false);

  void AddSeenForm(
      const FormData& form,
      const std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>&
          heuristic_types,
      const std::vector<FieldType>& server_types,
      bool preserve_values_in_form_structure = false);

  void AddSeenFormStructure(std::unique_ptr<FormStructure> form_structure);

  void ClearFormStructures();

  const std::string GetSubmittedFormSignature();

  // Helper to skip irrelevant params.
  void OnAskForValuesToFillTest(
      const FormData& form,
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kTextFieldDidChange);

  // Require a TestAutofillClient because `this` does not know whether its
  // `client()` is a *Test*AutofillClient.
  void SetAutofillProfileEnabled(TestAutofillClient& client,
                                 bool profile_enabled);
  void SetAutofillPaymentMethodsEnabled(TestAutofillClient& client,
                                        bool credit_card_enabled);

  void SetExpectedSubmittedFieldTypes(
      const std::vector<FieldTypeSet>& expected_types);

  void SetExpectedObservedSubmission(bool expected);

 private:
  bool autofill_profile_enabled_ = true;
  bool autofill_payment_methods_enabled_ = true;
  std::optional<bool> expected_observed_submission_;
  const gfx::Image card_image_ = gfx::test::CreateImage(40, 24);

  std::unique_ptr<base::RunLoop> run_loop_;

  std::string submitted_form_signature_;
  std::vector<FieldTypeSet> expected_submitted_field_types_;

  TestAutofillManagerWaiter waiter_{*this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_BROWSER_AUTOFILL_MANAGER_H_
