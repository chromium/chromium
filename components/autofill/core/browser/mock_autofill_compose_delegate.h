// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_COMPOSE_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_COMPOSE_DELEGATE_H_

#include <optional>

#include "components/autofill/core/browser/autofill_compose_delegate.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace url {
class Origin;
}  // namespace url

namespace autofill {

class MockAutofillComposeDelegate : public AutofillComposeDelegate {
 public:
  MockAutofillComposeDelegate();
  ~MockAutofillComposeDelegate() override;

  MOCK_METHOD(
      void,
      OpenCompose,
      (autofill::AutofillDriver&, FormGlobalId, FieldGlobalId, UiEntryPoint),
      (override));
  MOCK_METHOD(std::optional<autofill::Suggestion>,
              GetSuggestion,
              (const autofill::FormData&,
               const FormFieldData&,
               AutofillSuggestionTriggerSource),
              (override));
  MOCK_METHOD(void,
              NeverShowComposeForOrigin,
              (const url::Origin& origin),
              (override));
  MOCK_METHOD(void, DisableCompose, (), (override));
  MOCK_METHOD(void, GoToSettings, (), (override));
  MOCK_METHOD(bool, ShouldAnchorNudgeOnCaret, (), (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_COMPOSE_DELEGATE_H_
