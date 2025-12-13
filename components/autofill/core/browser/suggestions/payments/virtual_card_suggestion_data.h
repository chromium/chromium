// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_VIRTUAL_CARD_SUGGESTION_DATA_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_VIRTUAL_CARD_SUGGESTION_DATA_H_

#include <string>

#include "components/autofill/core/browser/data_model/payments/credit_card.h"

namespace autofill {

using VirtualCardSuggestionData = std::pair<CreditCard, std::u16string>;

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_PAYMENTS_VIRTUAL_CARD_SUGGESTION_DATA_H_
