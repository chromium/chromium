// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_IDENTITY_CREDENTIAL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_IDENTITY_CREDENTIAL_DELEGATE_H_

#include <vector>

#include "components/autofill/core/browser/autofill_field.h"

namespace autofill {

struct Suggestion;

// The interface for communication from //components/autofill to
// //content/browser/webid.
//
// The IdentityCredentialDelegate augments autofill with suggestions that are
// cryptographically signed by an Identity Provider.
//
// The suggestions come from FedCM Identity Providers, such as social networks
// and email providers, through FedCM's conditional requests (which are
// analogous to Passkey's conditional requests).
//
// For example, IdentityCredentialDelegate can augment the list of emails that
// the user may have in their autofill profile with a cryptographically signed
// email address from their email provider, which allows them to prove
// possession of the email without involving OTPs or magic-links.
class IdentityCredentialDelegate {
 public:
  virtual ~IdentityCredentialDelegate() = default;

  // Generates Verified Autofill suggestions.
  virtual std::vector<Suggestion> GetVerifiedAutofillSuggestions(
      const AutofillField& field) const = 0;
  virtual void NotifySuggestionAccepted(const Suggestion& suggestion) const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_IDENTITY_CREDENTIAL_DELEGATE_H_
