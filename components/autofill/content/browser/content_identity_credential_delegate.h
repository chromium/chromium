// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_IDENTITY_CREDENTIAL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_IDENTITY_CREDENTIAL_DELEGATE_H_

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/integrators/identity_credential_delegate.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// ContentIdentityCredentialDelegate is instantiated by the AutofillClient and
// therefore exists once per WebContents.
class ContentIdentityCredentialDelegate : public IdentityCredentialDelegate {
 public:
  explicit ContentIdentityCredentialDelegate(
      content::WebContents* web_contents);

  // Gets verified autofill suggestions from identity credentials requests.
  std::vector<Suggestion> GetVerifiedAutofillSuggestions(
      const AutofillField& field) const override;

  // Notifies the delegate that a suggestion from an identity credential
  // conditional request was accepted.
  void NotifySuggestionAccepted(const Suggestion& suggestion) const override;

 private:
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_IDENTITY_CREDENTIAL_DELEGATE_H_
