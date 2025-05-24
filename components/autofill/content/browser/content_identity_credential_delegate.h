// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_IDENTITY_CREDENTIAL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_IDENTITY_CREDENTIAL_DELEGATE_H_

#include "base/functional/callback.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/integrators/identity_credential/identity_credential_delegate.h"
#include "content/public/browser/federated_auth_autofill_source.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

// ContentIdentityCredentialDelegate is instantiated by the AutofillClient and
// therefore exists once per WebContents.
class ContentIdentityCredentialDelegate : public IdentityCredentialDelegate {
 public:
  explicit ContentIdentityCredentialDelegate(
      content::WebContents* web_contents);

  // Exposed for tests to inject a mock `FederatedAuthAutofillSource` as a
  // dependency.
  explicit ContentIdentityCredentialDelegate(
      base::RepeatingCallback<content::FederatedAuthAutofillSource*()> source);

  ~ContentIdentityCredentialDelegate() override;

  std::vector<Suggestion> GetVerifiedAutofillSuggestions(
      const FieldType& field_type) const override;

  void NotifySuggestionAccepted(
      const Suggestion& suggestion,
      bool show_modal,
      OnFederatedTokenReceivedCallback callback) const override;

 private:
  // Provides a `FederatedAuthAutofillSource`. Derived from `WebContents` in
  // practice and mocked in tests.
  base::RepeatingCallback<content::FederatedAuthAutofillSource*()> source_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_CONTENT_IDENTITY_CREDENTIAL_DELEGATE_H_
