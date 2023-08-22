// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_browser_util.h"

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/form_structure.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace {
// Matches the blink check for mixed content.
bool IsInsecureFormAction(const GURL& action_url) {
  // blob: and filesystem: URLs never hit the network, and access is restricted
  // to same-origin contexts, so they are not blocked. Some forms use
  // javascript URLs to handle submissions in JS, those don't count as mixed
  // content either.
  if (action_url.SchemeIs(url::kJavaScriptScheme) ||
      action_url.SchemeIs(url::kBlobScheme) ||
      action_url.SchemeIs(url::kFileSystemScheme)) {
    return false;
  }
  return !network::IsUrlPotentiallyTrustworthy(action_url);
}
}  // namespace

namespace autofill {

bool IsFormOrClientNonSecure(const AutofillClient& client,
                             const FormData& form) {
  return !client.IsContextSecure() ||
         (form.action.is_valid() && form.action.SchemeIs("http"));
}

bool IsFormOrClientNonSecure(const AutofillClient& client,
                             const FormStructure& form) {
  return !client.IsContextSecure() ||
         (form.target_url().is_valid() && form.target_url().SchemeIs("http"));
}

bool IsFormMixedContent(const AutofillClient& client, const FormData& form) {
  return client.IsContextSecure() &&
         (form.action.is_valid() && IsInsecureFormAction(form.action));
}

bool ShouldAllowCreditCardFallbacks(const AutofillClient& client,
                                    const FormData& form) {
  // Skip the form check if there wasn't a form yet:
  if (form.unique_renderer_id.is_null())
    return client.IsContextSecure();
  return !IsFormOrClientNonSecure(client, form);
}

}  // namespace autofill
