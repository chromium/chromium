// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_MANAGER_H_

#include <string>

#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class FormData;
class FormStructure;
class AutofillDriver;

// Reusable mock of AutofillManager. Note that only the pure virtual methods are
// mocked here; non-virtual methods still rely on their default implementation.
class MockAutofillManager : public AutofillManager {
 public:
  explicit MockAutofillManager(AutofillDriver* driver);
  MockAutofillManager(const MockAutofillManager&) = delete;
  MockAutofillManager& operator=(const MockAutofillManager&) = delete;
  ~MockAutofillManager() override;

  MOCK_METHOD(bool, ShouldClearPreviewedForm, (), (override));
  MOCK_METHOD(void, OnFocusOnNonFormFieldImpl, (), (override));
  MOCK_METHOD(void,
              OnDidFillAutofillFormDataImpl,
              (const FormData& form, const base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void, OnDidEndTextFieldEditingImpl, (), (override));
  MOCK_METHOD(void, OnHidePopupImpl, (), (override));
  MOCK_METHOD(void,
              OnSelectFieldOptionsDidChangeImpl,
              (const FormData& form),
              (override));
  MOCK_METHOD(void,
              OnJavaScriptChangedAutofilledValueImpl,
              (const FormData& form,
               const FieldGlobalId& field_id,
               const std::u16string& old_value,
               bool formatting_only),
              (override));
  MOCK_METHOD(void,
              OnFormSubmittedImpl,
              (const FormData& form,
               bool known_success,
               mojom::SubmissionSource source),
              (override));
  MOCK_METHOD(void,
              OnCaretMovedInFormFieldImpl,
              (const FormData& form,
               const FieldGlobalId& field_id,
               const gfx::Rect& caret_bounds),
              (override));
  MOCK_METHOD(void,
              OnTextFieldDidChangeImpl,
              (const FormData& form,
               const FieldGlobalId& field_id,
               const base::TimeTicks timestamp),
              (override));
  MOCK_METHOD(void,
              OnTextFieldDidScrollImpl,
              (const FormData& form, const FieldGlobalId& field_id),
              (override));
  MOCK_METHOD(void,
              OnAskForValuesToFillImpl,
              (const FormData& form,
               const FieldGlobalId& field_id,
               const gfx::Rect& caret_bounds,
               AutofillSuggestionTriggerSource trigger_source),
              (override));
  MOCK_METHOD(void,
              OnFocusOnFormFieldImpl,
              (const FormData& form, const FieldGlobalId& field_id),
              (override));
  MOCK_METHOD(void,
              OnSelectControlDidChangeImpl,
              (const FormData& form, const FieldGlobalId& field_id),
              (override));
  MOCK_METHOD(bool, ShouldParseForms, (), (override));
  MOCK_METHOD(void, OnBeforeProcessParsedForms, (), (override));
  MOCK_METHOD(void,
              OnFormProcessed,
              (const FormData& form_data, const FormStructure& form_structure),
              (override));
  MOCK_METHOD(void,
              ReportAutofillWebOTPMetrics,
              (bool used_web_otp),
              (override));

  base::WeakPtr<AutofillManager> GetWeakPtr() override;

 private:
  base::WeakPtrFactory<MockAutofillManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_MANAGER_H_
