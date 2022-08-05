// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_BROWSER_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_BROWSER_AUTOFILL_MANAGER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

class TestAutofillClient;
class TestAutofillDriver;
class FormStructure;
class TestPersonalDataManager;

class TestBrowserAutofillManager : public BrowserAutofillManager {
 public:
  TestBrowserAutofillManager(TestAutofillDriver* driver,
                             TestAutofillClient* client);

  TestBrowserAutofillManager(const TestBrowserAutofillManager&) = delete;
  TestBrowserAutofillManager& operator=(const TestBrowserAutofillManager&) =
      delete;

  ~TestBrowserAutofillManager() override;

  TestAutofillClient* client() { return client_; }
  TestAutofillDriver* driver() { return driver_; }

  // AutofillManager overrides.
  // The overrides ensure that the thread is blocked until the form has been
  // parsed (perhaps asynchronously, depending on AutofillParseAsync).
  void OnLanguageDetermined(
      const translate::LanguageDetectionDetails& details) override;
  void OnFormsSeen(const std::vector<FormData>& updated_forms,
                   const std::vector<FormGlobalId>& removed_forms) override;
  void OnTextFieldDidChange(const FormData& form,
                            const FormFieldData& field,
                            const gfx::RectF& bounding_box,
                            const base::TimeTicks timestamp) override;
  void OnDidFillAutofillFormData(const FormData& form,
                                 const base::TimeTicks timestamp) override;
  void OnAskForValuesToFill(
      const FormData& form,
      const FormFieldData& field,
      const gfx::RectF& bounding_box,
      int query_id,
      bool autoselect_first_suggestion,
      TouchToFillEligible touch_to_fill_eligible) override;
  void OnJavaScriptChangedAutofilledValue(
      const FormData& form,
      const FormFieldData& field,
      const std::u16string& old_value) override;
  void OnFormSubmitted(const FormData& form,
                       const bool known_success,
                       const mojom::SubmissionSource source) override;

  // BrowserAutofillManager overrides.
  bool IsAutofillProfileEnabled() const override;
  bool IsAutofillCreditCardEnabled() const override;
  void UploadFormData(const FormStructure& submitted_form,
                      bool observed_submission) override;
  bool MaybeStartVoteUploadProcess(
      std::unique_ptr<FormStructure> form_structure,
      bool observed_submission) override;
  void UploadFormDataAsyncCallback(const FormStructure* submitted_form,
                                   const base::TimeTicks& interaction_time,
                                   const base::TimeTicks& submission_time,
                                   bool observed_submission) override;
  // Immediately triggers the refill.
  void ScheduleRefill(const FormData& form) override;

  // Unique to TestBrowserAutofillManager:

  int GetPackedCreditCardID(int credit_card_id);

  void AddSeenForm(const FormData& form,
                   const std::vector<ServerFieldType>& heuristic_types,
                   const std::vector<ServerFieldType>& server_types,
                   bool preserve_values_in_form_structure = false);

  void AddSeenForm(
      const FormData& form,
      const std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>&
          heuristic_types,
      const std::vector<ServerFieldType>& server_types,
      bool preserve_values_in_form_structure = false);

  void SetSeenFormPredictions(
      FormGlobalId form_id,
      const std::vector<ServerFieldType>& heuristic_types,
      const std::vector<ServerFieldType>& server_types);

  void SetSeenFormPredictions(
      FormGlobalId form_id,
      const std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>&
          heuristic_types,
      const std::vector<ServerFieldType>& server_types);

  void AddSeenFormStructure(std::unique_ptr<FormStructure> form_structure);

  void ClearFormStructures();

  const std::string GetSubmittedFormSignature();

  // Helper to skip irrelevant params.
  void OnAskForValuesToFillTest(
      const FormData& form,
      const FormFieldData& field,
      int query_id = 0,
      const gfx::RectF& bounding_box = {},
      bool autoselect_first_suggestion = false,
      TouchToFillEligible touch_to_fill_eligible = TouchToFillEligible(false));

  void SetAutofillProfileEnabled(bool profile_enabled);

  void SetAutofillCreditCardEnabled(bool credit_card_enabled);

  void SetExpectedSubmittedFieldTypes(
      const std::vector<ServerFieldTypeSet>& expected_types);

  void SetExpectedObservedSubmission(bool expected);

  void SetCallParentUploadFormData(bool value);

  using BrowserAutofillManager::pending_form_data;

 private:
  raw_ptr<TestAutofillClient> client_;
  raw_ptr<TestAutofillDriver> driver_;

  bool autofill_profile_enabled_ = true;
  bool autofill_credit_card_enabled_ = true;
  bool call_parent_upload_form_data_ = false;
  absl::optional<bool> expected_observed_submission_;

  std::unique_ptr<base::RunLoop> run_loop_;

  std::string submitted_form_signature_;
  std::vector<ServerFieldTypeSet> expected_submitted_field_types_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_BROWSER_AUTOFILL_MANAGER_H_
