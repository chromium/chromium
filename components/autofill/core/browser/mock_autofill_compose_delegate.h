// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_COMPOSE_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_COMPOSE_DELEGATE_H_

#include "components/autofill/core/browser/autofill_compose_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockAutofillComposeDelegate : public AutofillComposeDelegate {
 public:
  MockAutofillComposeDelegate();
  ~MockAutofillComposeDelegate() override;

  MOCK_METHOD(bool,
              ShouldOfferComposePopup,
              (const FormFieldData&),
              (override));
  MOCK_METHOD(void,
              OpenCompose,
              (UiEntryPoint,
               const FormFieldData&,
               std::optional<AutofillClient::PopupScreenLocation>,
               ComposeCallback),
              (override));
  MOCK_METHOD(bool, HasSavedState, (const FieldGlobalId&), (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_MOCK_AUTOFILL_COMPOSE_DELEGATE_H_
