// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_browser_util.h"

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/security_interstitials/core/insecure_form_util.h"

namespace autofill {

bool IsFormOrClientNonSecure(const AutofillClient& client,
                             const FormData& form) {
  return !client.IsContextSecure() ||
         (form.action().is_valid() && form.action().SchemeIs("http"));
}

bool IsFormOrClientNonSecure(const AutofillClient& client,
                             const FormStructure& form) {
  return !client.IsContextSecure() ||
         (form.target_url().is_valid() && form.target_url().SchemeIs("http"));
}

bool IsFormMixedContent(const AutofillClient& client, const FormData& form) {
  return client.IsContextSecure() &&
         (form.action().is_valid() &&
          security_interstitials::IsInsecureFormAction(form.action()));
}

bool ShouldAllowCreditCardFallbacks(const AutofillClient& client,
                                    const FormData& form) {
  // Skip the form check if there wasn't a form yet:
  if (form.renderer_id().is_null()) {
    return client.IsContextSecure();
  }
  return !IsFormOrClientNonSecure(client, form);
}

}  // namespace autofill
