// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_browser_util.h"

#include "components/autofill/core/browser/autofill_client.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace {
// Matches the blink check for mixed content.
bool IsInsecureFormAction(const GURL& action_url) {
  // blob: and filesystem: URLs never hit the network, and access is restricted
  // to same-origin contexts, so they are not blocked. Some forms use
  // javascript URLs to handle submissions in JS, those don't count as mixed
  // content either.
  // The data scheme is explicitly allowed in order to match blink's equivalent
  // check, since IsUrlPotentiallyTrustworthy excludes it.
  if (action_url.SchemeIs(url::kJavaScriptScheme) ||
      action_url.SchemeIs(url::kBlobScheme) ||
      action_url.SchemeIs(url::kFileSystemScheme) ||
      action_url.SchemeIs(url::kDataScheme)) {
    return false;
  }
  return !network::IsUrlPotentiallyTrustworthy(action_url);
}
}  // namespace

namespace autofill {

bool IsFormOrClientNonSecure(AutofillClient* client, const FormData& form) {
  return !client->IsContextSecure() ||
         (form.action.is_valid() && form.action.SchemeIs("http"));
}

bool IsFormMixedContent(AutofillClient* client, const FormData& form) {
  return client->IsContextSecure() &&
         (form.action.is_valid() && IsInsecureFormAction(form.action));
}

bool ShouldAllowCreditCardFallbacks(AutofillClient* client,
                                    const FormData& form) {
  // Skip the form check if there wasn't a form yet:
  if (form.unique_renderer_id.is_null())
    return client->IsContextSecure();
  return !IsFormOrClientNonSecure(client, form);
}

}  // namespace autofill
