// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_BROWSER_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_BROWSER_UTIL_H_

#include <stddef.h>

#include "components/autofill/core/common/form_data.h"

namespace autofill {

class AutofillClient;

// Checks whether a given form is considered insecure (by origin or action).
bool IsFormOrClientNonSecure(AutofillClient* client, const FormData& form);

// Returns true if context provided by the client and the given form are
// considered "secure enough" to manually fill credit card data.
bool ShouldAllowCreditCardFallbacks(AutofillClient* client,
                                    const FormData& form);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_BROWSER_UTIL_H_
