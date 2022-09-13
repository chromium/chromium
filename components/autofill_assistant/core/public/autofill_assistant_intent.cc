// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/core/public/autofill_assistant_intent.h"

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"

namespace autofill_assistant {

namespace {

// The list of intents that is known at compile-time. Intents not in this list
// will be recorded as UNDEFINED_INTENT.
constexpr auto intents = base::MakeFixedFlatMap<base::StringPiece,
                                                AutofillAssistantIntent>(
    {{"BUY_MOVIE_TICKET", AutofillAssistantIntent::BUY_MOVIE_TICKET},
     {"RENT_CAR", AutofillAssistantIntent::RENT_CAR},
     {"SHOPPING", AutofillAssistantIntent::SHOPPING},
     {"TELEPORT", AutofillAssistantIntent::TELEPORT},
     {"SHOPPING_ASSISTED_CHECKOUT",
      AutofillAssistantIntent::SHOPPING_ASSISTED_CHECKOUT},
     {"FLIGHTS_CHECKIN", AutofillAssistantIntent::FLIGHTS_CHECKIN},
     {"FOOD_ORDERING", AutofillAssistantIntent::FOOD_ORDERING},
     {"PASSWORD_CHANGE", AutofillAssistantIntent::PASSWORD_CHANGE},
     {"FOOD_ORDERING_PICKUP", AutofillAssistantIntent::FOOD_ORDERING_PICKUP},
     {"FOOD_ORDERING_DELIVERY",
      AutofillAssistantIntent::FOOD_ORDERING_DELIVERY},
     {"UNLAUNCHED_VERTICAL_1", AutofillAssistantIntent::UNLAUNCHED_VERTICAL_1},
     {"FIND_COUPONS", AutofillAssistantIntent::FIND_COUPONS},
     {"CHROME_FAST_CHECKOUT", AutofillAssistantIntent::CHROME_FAST_CHECKOUT}});

}  // namespace

AutofillAssistantIntent ExtractIntentFromString(
    const absl::optional<std::string>& intent) {
  if (intent && base::Contains(intents, *intent)) {
    return intents.at(*intent);
  }

  return AutofillAssistantIntent::UNDEFINED_INTENT;
}

}  // namespace autofill_assistant
