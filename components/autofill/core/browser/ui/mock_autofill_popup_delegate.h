// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_AUTOFILL_POPUP_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_AUTOFILL_POPUP_DELEGATE_H_

#include "components/autofill/core/browser/ui/autofill_popup_delegate.h"

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

// Mock version of AutofillPopupDelegate.
class MockAutofillPopupDelegate : public AutofillPopupDelegate {
 public:
  MockAutofillPopupDelegate();
  ~MockAutofillPopupDelegate() override;

  MOCK_METHOD(void, OnPopupShown, (), (override));
  MOCK_METHOD(void, OnPopupHidden, (), (override));
  MOCK_METHOD(void, OnPopupSuppressed, (), (override));
  MOCK_METHOD(void,
              DidSelectSuggestion,
              (const Suggestion& suggestion,
               AutofillSuggestionTriggerSource trigger_source),
              (override));
  MOCK_METHOD(void,
              DidAcceptSuggestion,
              (const Suggestion& suggestion,
               int position,
               AutofillSuggestionTriggerSource trigger_source),
              (override));
  MOCK_METHOD(bool,
              GetDeletionConfirmationText,
              (const std::u16string& value,
               PopupItemId popup_item_id,
               Suggestion::BackendId backend_id,
               std::u16string* title,
               std::u16string* body),
              (override));
  MOCK_METHOD(bool,
              RemoveSuggestion,
              (const std::u16string& value,
               PopupItemId popup_item_id,
               Suggestion::BackendId backend_id),
              (override));
  MOCK_METHOD(void, ClearPreviewedForm, (), (override));
  MOCK_METHOD(PopupType, GetPopupType, (), (const, override));
  MOCK_METHOD((absl::variant<AutofillDriver*,
                             password_manager::PasswordManagerDriver*>),
              GetDriver,
              (),
              (override));
  MOCK_METHOD(int32_t,
              GetWebContentsPopupControllerAxId,
              (),
              (const, override));
  MOCK_METHOD(void,
              RegisterDeletionCallback,
              (base::OnceClosure deletion_callback),
              (override));

  base::WeakPtr<MockAutofillPopupDelegate> GetWeakPtr();

 private:
  base::WeakPtrFactory<MockAutofillPopupDelegate> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_MOCK_AUTOFILL_POPUP_DELEGATE_H_
