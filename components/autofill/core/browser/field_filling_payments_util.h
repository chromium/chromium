// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLING_PAYMENTS_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLING_PAYMENTS_UTIL_H_

#include <optional>
#include <string>

#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

class AutofillField;
class CreditCard;

// Returns the appropriate `credit_card` value based on `field_type` to fill
// into `field`, and nullopt if no value could be found for the given `field`.
std::optional<std::u16string> GetValueForCreditCard(
    const CreditCard& credit_card,
    const std::u16string& cvc,
    const std::string& app_locale,
    mojom::ActionPersistence action_persistence,
    const AutofillField& field,
    std::string* failure_to_fill);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FIELD_FILLING_PAYMENTS_UTIL_H_
