// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_IBAN_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_IBAN_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/iban_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockIbanManager : public IbanManager {
 public:
  explicit MockIbanManager(PersonalDataManager* personal_data_manager);

  ~MockIbanManager() override;

  MOCK_METHOD(bool,
              OnGetSingleFieldSuggestions,
              (AutofillSuggestionTriggerSource trigger_source,
               const FormFieldData& field,
               const AutofillClient& client,
               base::WeakPtr<IbanManager::SuggestionsHandler> handler,
               const SuggestionsContext& context),
              (override));
  MOCK_METHOD(void,
              OnWillSubmitFormWithFields,
              (const std::vector<FormFieldData>& fields,
               bool is_autocomplete_enabled),
              (override));
  MOCK_METHOD(void,
              CancelPendingQueries,
              (const IbanManager::SuggestionsHandler*),
              (override));
  MOCK_METHOD(void,
              OnRemoveCurrentSingleFieldSuggestion,
              (const std::u16string&, const std::u16string&, PopupItemId),
              (override));
  MOCK_METHOD(void,
              OnSingleFieldSuggestionSelected,
              (const std::u16string&, PopupItemId),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_IBAN_MANAGER_H_
