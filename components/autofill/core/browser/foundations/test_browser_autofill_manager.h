// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_BROWSER_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_BROWSER_AUTOFILL_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/filling/test_form_filler.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/payments/test/mock_bnpl_manager.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace autofill {

class MockBnplManager;
class AutofillDriver;
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
  void OnTextFieldValueChanged(const FormData& form,
                               const FieldGlobalId& field_id,
                               const base::TimeTicks timestamp) override;
  void OnDidEndTextFieldEditing() override;
  void OnTextFieldDidScroll(const FormData& form,
                            const FieldGlobalId& field_id) override;
  void OnSelectControlSelectionChanged(const FormData& form,
                                       const FieldGlobalId& field_id) override;
  void OnSelectFieldOptionsDidChange(const FormData& form,
                                     const FieldGlobalId& field_id) override;
  void OnAskForValuesToFill(
      const FormData& form,
      const FieldGlobalId& field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source,
      std::optional<PasswordSuggestionRequest> password_request) override;
  void OnFocusOnFormField(const FormData& form,
                          const FieldGlobalId& field_id) override;
  void OnDidAutofillForm(const FormData& form) override;
  void OnJavaScriptChangedAutofilledValue(
      const FormData& form,
      const FieldGlobalId& field_id,
      const std::u16string& old_value) override;
  void OnFormSubmitted(const FormData& form,
                       const mojom::SubmissionSource source) override;

  // BrowserAutofillManager overrides.
  const gfx::Image& GetCardImage(const CreditCard& credit_card) override;
  testing::NiceMock<MockBnplManager>* GetPaymentsBnplManager() override;

  // Unique to TestBrowserAutofillManager:

  void AddSeenForm(const FormData& form,
                   const std::vector<FieldType>& field_types) {
    AddSeenForm(form, /*heuristic_types=*/field_types,
                /*server_types=*/field_types);
  }

  void AddSeenForm(const FormData& form,
                   const std::vector<FieldType>& heuristic_types,
                   const std::vector<FieldType>& server_types);

  void AddSeenForm(
      const FormData& form,
      const std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>&
          heuristic_types,
      const std::vector<FieldType>& server_types);

  const std::string& GetSubmittedFormSignature();

  // Helper to skip irrelevant params.
  void OnAskForValuesToFillTest(
      const FormData& form,
      const FieldGlobalId& field_id,
      AutofillSuggestionTriggerSource trigger_source =
          AutofillSuggestionTriggerSource::kTextFieldValueChanged,
      std::optional<PasswordSuggestionRequest> password_request = std::nullopt);

 private:
  const gfx::Image card_image_ = gfx::test::CreateImage(40, 24);

  testing::NiceMock<MockBnplManager> mock_bnpl_manager_{this};

  TestAutofillManagerWaiter waiter_{*this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_TEST_BROWSER_AUTOFILL_MANAGER_H_
