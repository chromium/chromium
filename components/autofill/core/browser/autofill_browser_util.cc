// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_browser_util.h"

#include "components/autofill/core/browser/autofill_client.h"

namespace autofill {

bool IsFormOrClientNonSecure(AutofillClient* client, const FormData& form) {
  return !client->IsContextSecure() ||
         (form.action.is_valid() && form.action.SchemeIs("http"));
}

bool ShouldAllowCreditCardFallbacks(AutofillClient* client,
                                    const FormData& form) {
  // Skip the form check if there wasn't a form yet:
  if (form.unique_renderer_id == FormData::kNotSetFormRendererId)
    return client->IsContextSecure();
  return !IsFormOrClientNonSecure(client, form);
}

}  // namespace autofill
