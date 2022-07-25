// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_CORE_PUBLIC_AUTOFILL_ASSISTANT_INTENT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_CORE_PUBLIC_AUTOFILL_ASSISTANT_INTENT_H_

#include <string>
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {
// Used for logging the intent of an autofill-assistant flow.
//
// This enum is used in UKM metrics, do not remove/renumber entries. Only add
// at the end and update kMaxValue. Also remember to update the
// AutofillAssistantIntent enum listing in
// tools/metrics/histograms/enums.xml.
enum class AutofillAssistantIntent {
  UNDEFINED_INTENT = 0,
  BUY_MOVIE_TICKET = 3,
  RENT_CAR = 9,
  SHOPPING = 10,
  TELEPORT = 11,
  SHOPPING_ASSISTED_CHECKOUT = 14,
  FLIGHTS_CHECKIN = 15,
  FOOD_ORDERING = 17,
  PASSWORD_CHANGE = 18,
  FOOD_ORDERING_PICKUP = 19,
  FOOD_ORDERING_DELIVERY = 20,
  UNLAUNCHED_VERTICAL_1 = 22,
  FIND_COUPONS = 25,
  CHROME_FAST_CHECKOUT = 32,

  kMaxValue = CHROME_FAST_CHECKOUT
};

// Extracts the enum value corresponding to |intent|.
AutofillAssistantIntent ExtractIntentFromString(
    const absl::optional<std::string>& intent);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_INTENT_H_
