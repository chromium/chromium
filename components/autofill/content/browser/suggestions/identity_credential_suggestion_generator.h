
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_SUGGESTIONS_IDENTITY_CREDENTIAL_SUGGESTION_GENERATOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_SUGGESTIONS_IDENTITY_CREDENTIAL_SUGGESTION_GENERATOR_H_

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_generator.h"
#include "content/public/browser/webid/autofill_source.h"

namespace autofill {

class IdentityCredentialSuggestionGenerator : public SuggestionGenerator {
 public:
  explicit IdentityCredentialSuggestionGenerator(
      base::RepeatingCallback<content::webid::AutofillSource*()> source);
  ~IdentityCredentialSuggestionGenerator() override;

  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::OnceCallback<void(ReturnedSuggestions)> callback) override;

  // Like SuggestionGenerator override, but takes a base::FunctionRef instead of
  // a base::OnceCallback. Calls that callback exactly once.
  void GenerateSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      const FormStructure* form_structure,
      const AutofillField* trigger_autofill_field,
      const AutofillClient& client,
      base::FunctionRef<void(ReturnedSuggestions)> callback);

 private:
  // Provides a `AutofillSource`. Derived from `WebContents` in
  // practice and mocked in tests.
  base::RepeatingCallback<content::webid::AutofillSource*()> source_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_SUGGESTIONS_IDENTITY_CREDENTIAL_SUGGESTION_GENERATOR_H_
