// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_EMAIL_VERIFIER_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_EMAIL_VERIFIER_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"

namespace content::webid {
class EmailVerifier;
}  // namespace content::webid

namespace autofill {

class AutofillClient;

// The EmailVerifierDelegate is owned by the ChromeAutofillClient (and hence is
// one per WebContents) and observes all AutofillManagers.
// It listens to filling events and triggers the email verification flow if an
// participating input field (one that has a "nonce" attribute) is filled with
// an email address.
//
// https://github.com/dickhardt/email-verification-protocol
class EmailVerifierDelegate : public AutofillManager::Observer {
 public:
  using EmailVerifierBuilder =
      base::RepeatingCallback<content::webid::EmailVerifier*(AutofillManager&)>;

  explicit EmailVerifierDelegate(AutofillClient* client);
  EmailVerifierDelegate(AutofillClient* client, EmailVerifierBuilder builder);
  EmailVerifierDelegate(const EmailVerifierDelegate&) = delete;
  EmailVerifierDelegate& operator=(const EmailVerifierDelegate&) = delete;

  ~EmailVerifierDelegate() override;

  // AutofillManager::Observer:
  void OnFillOrPreviewForm(
      AutofillManager& manager,
      FormGlobalId form_id,
      mojom::ActionPersistence action_persistence,
      const base::flat_set<FieldGlobalId>& filled_field_ids,
      const FillingPayload& filling_payload) override;

 private:
  EmailVerifierBuilder email_verifier_builder_;

  ScopedAutofillManagersObservation observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_EMAIL_VERIFIER_DELEGATE_H_
