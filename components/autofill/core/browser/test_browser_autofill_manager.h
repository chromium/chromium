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
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/filling/test_form_filler.h"
#include "components/autofill/core/browser/test_autofill_manager_waiter.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {

class AutofillDriver;
class FormStructure;
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
                       const mojom::SubmissionSource source) override;

  // BrowserAutofillManager overrides.
  const gfx::Image& GetCardImage(const CreditCard& credit_card) override;

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

  const std::string& GetSubmittedFormSignature();

  // Helper to skip irrelevant params.
  void OnAskForValuesToFillTest(
      const FormData& form,
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kTextFieldDidChange);

 private:
  const gfx::Image card_image_ = gfx::test::CreateImage(40, 24);

  TestAutofillManagerWaiter waiter_{*this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_BROWSER_AUTOFILL_MANAGER_H_
