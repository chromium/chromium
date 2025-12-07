// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_IDENTITY_CREDENTIAL_MOCK_IDENTITY_CREDENTIAL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_IDENTITY_CREDENTIAL_MOCK_IDENTITY_CREDENTIAL_DELEGATE_H_

#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "components/autofill/core/common/form_field_data.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockIdentityCredentialDelegate : public IdentityCredentialDelegate {
 public:
  MockIdentityCredentialDelegate();
  ~MockIdentityCredentialDelegate() override;

  MOCK_METHOD(std::vector<Suggestion>,
              GetVerifiedAutofillSuggestions,
              (const FormData& form,
               const FormStructure* form_structure,
               const FormFieldData& field,
               const AutofillField* autofill_field,
               const AutofillClient& client),
              (const override));
  MOCK_METHOD(void,
              NotifySuggestionAccepted,
              (const Suggestion& suggestion,
               bool show_modal,
               OnFederatedTokenReceivedCallback callback),
              (const override));
  MOCK_METHOD(std::unique_ptr<SuggestionGenerator>,
              GetIdentityCredentialSuggestionGenerator,
              (),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_IDENTITY_CREDENTIAL_MOCK_IDENTITY_CREDENTIAL_DELEGATE_H_
