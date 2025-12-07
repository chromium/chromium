// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_MOCK_AUTOFILL_MANAGER_OBSERVER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_MOCK_AUTOFILL_MANAGER_OBSERVER_H_

#include "base/containers/span.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillManagerObserver : public AutofillManager::Observer {
 public:
  MockAutofillManagerObserver();
  MockAutofillManagerObserver(const MockAutofillManagerObserver&) = delete;
  MockAutofillManagerObserver& operator=(const MockAutofillManagerObserver&) =
      delete;
  ~MockAutofillManagerObserver() override;

  MOCK_METHOD(void,
              OnAutofillManagerStateChanged,
              (AutofillManager&,
               AutofillManager::LifecycleState,
               AutofillManager::LifecycleState),
              (override));

  MOCK_METHOD(void, OnBeforeLanguageDetermined, (AutofillManager&), (override));
  MOCK_METHOD(void, OnAfterLanguageDetermined, (AutofillManager&), (override));

  MOCK_METHOD(void,
              OnBeforeFormsSeen,
              (AutofillManager&,
               base::span<const FormGlobalId>,
               base::span<const FormGlobalId>),
              (override));
  MOCK_METHOD(void,
              OnAfterFormsSeen,
              (AutofillManager&,
               base::span<const FormGlobalId>,
               base::span<const FormGlobalId>),
              (override));

  MOCK_METHOD(void,
              OnBeforeTextFieldValueChanged,
              (AutofillManager&, FormGlobalId, FieldGlobalId),
              (override));
  MOCK_METHOD(
      void,
      OnAfterTextFieldValueChanged,
      (AutofillManager&, FormGlobalId, FieldGlobalId, const std::u16string&),
      (override));

  MOCK_METHOD(void,
              OnBeforeTextFieldDidScroll,
              (AutofillManager&, FormGlobalId, FieldGlobalId),
              (override));
  MOCK_METHOD(void,
              OnAfterTextFieldDidScroll,
              (AutofillManager&, FormGlobalId, FieldGlobalId),
              (override));

  MOCK_METHOD(void,
              OnBeforeSelectControlSelectionChanged,
              (AutofillManager&, FormGlobalId, FieldGlobalId),
              (override));
  MOCK_METHOD(void,
              OnAfterSelectControlSelectionChanged,
              (AutofillManager&, FormGlobalId, FieldGlobalId),
              (override));

  MOCK_METHOD(void,
              OnBeforeSelectFieldOptionsDidChange,
              (AutofillManager&, FormGlobalId),
              (override));
  MOCK_METHOD(void,
              OnAfterSelectFieldOptionsDidChange,
              (AutofillManager&, FormGlobalId),
              (override));

  MOCK_METHOD(void,
              OnBeforeDidAutofillForm,
              (AutofillManager&, FormGlobalId),
              (override));
  MOCK_METHOD(void,
              OnAfterDidAutofillForm,
              (AutofillManager&, FormGlobalId),
              (override));

  MOCK_METHOD(void,
              OnBeforeAskForValuesToFill,
              (AutofillManager&, FormGlobalId, FieldGlobalId, const FormData&),
              (override));
  MOCK_METHOD(void,
              OnAfterAskForValuesToFill,
              (AutofillManager&, FormGlobalId, FieldGlobalId),
              (override));

  MOCK_METHOD(void,
              OnBeforeFocusOnFormField,
              (AutofillManager&, FormGlobalId, FieldGlobalId),
              (override));
  MOCK_METHOD(void,
              OnAfterFocusOnFormField,
              (AutofillManager&, FormGlobalId, FieldGlobalId),
              (override));

  MOCK_METHOD(void,
              OnBeforeFocusOnNonFormField,
              (AutofillManager&),
              (override));
  MOCK_METHOD(void, OnAfterFocusOnNonFormField, (AutofillManager&), (override));

  MOCK_METHOD(void,
              OnBeforeJavaScriptChangedAutofilledValue,
              (AutofillManager&, FormGlobalId, FieldGlobalId),
              (override));
  MOCK_METHOD(void,
              OnAfterJavaScriptChangedAutofilledValue,
              (AutofillManager&, FormGlobalId, FieldGlobalId),
              (override));

  MOCK_METHOD(void,
              OnBeforeLoadedServerPredictions,
              (AutofillManager&),
              (override));
  MOCK_METHOD(void,
              OnAfterLoadedServerPredictions,
              (AutofillManager&),
              (override));

  MOCK_METHOD(void,
              OnFieldTypesDetermined,
              (AutofillManager&, FormGlobalId, FieldTypeSource),
              (override));

  MOCK_METHOD(void,
              OnFillOrPreviewForm,
              (AutofillManager&,
               FormGlobalId,
               mojom::ActionPersistence,
               (const base::flat_set<FieldGlobalId>&),
               (const FillingPayload&)),
              (override));

  MOCK_METHOD(void,
              OnBeforeFormSubmitted,
              (AutofillManager&, const FormData&),
              (override));
  MOCK_METHOD(void,
              OnAfterFormSubmitted,
              (AutofillManager&, const FormData&),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FOUNDATIONS_MOCK_AUTOFILL_MANAGER_OBSERVER_H_
