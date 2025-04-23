// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_IDENTITY_CREDENTIAL_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_IDENTITY_CREDENTIAL_DELEGATE_H_

#include <vector>

#include "base/functional/callback.h"
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
  // For 3P logins usually the identity provider needs to issue a federated
  // token such as ID token, access token etc. to complete the flow. This
  // callback is triggered when the browser receives such a token.
  using OnFederatedTokenReceivedCallback = base::OnceClosure;
  virtual ~IdentityCredentialDelegate() = default;

  // Generates verified Autofill suggestions from identity credential requests.
  // This could be representing two types of identity credentials suggestions:
  // 1. Verified email and potentially other attributes from an identity
  // provider, e.g. name, address etc.
  // 2. Accounts information that is required for federated logins. e.g. name,
  // email, avatar, phone number etc. Depending on which types of suggestions,
  // the strings and UI affordances can be different.
  virtual std::vector<Suggestion> GetVerifiedAutofillSuggestions(
      const FieldType& field_type) const = 0;

  // Notifies the delegate that a suggestion from an identity credential
  // conditional request was accepted. After a user selects the suggestion from
  // the autofill dropdown UI, we should enter a loading state similar to the
  // selecting passkeys UX.The callback will be called when a federated token is
  // received. Once it's called, the loading menu will be hidden.
  virtual void NotifySuggestionAccepted(
      const Suggestion& suggestion,
      OnFederatedTokenReceivedCallback callback) const = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_INTEGRATORS_IDENTITY_CREDENTIAL_DELEGATE_H_
