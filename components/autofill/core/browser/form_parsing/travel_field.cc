// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/travel_field.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

TravelField::~TravelField() = default;

// static
std::unique_ptr<FormField> TravelField::Parse(AutofillScanner* scanner,
                                              LogManager* log_manager) {
  if (!scanner || scanner->IsEnd()) {
    return nullptr;
  }

  auto travel_field = std::make_unique<TravelField>();
  if (ParseField(scanner, base::UTF8ToUTF16(kPassportRe),
                 &travel_field->passport_, {log_manager, "kPassportRe"}) ||
      ParseField(scanner, base::UTF8ToUTF16(kTravelOriginRe),
                 &travel_field->origin_, {log_manager, "kTravelOriginRe"}) ||
      ParseField(scanner, base::UTF8ToUTF16(kTravelDestinationRe),
                 &travel_field->destination_,
                 {log_manager, "kTravelDestinationRe"}) ||
      ParseField(scanner, base::UTF8ToUTF16(kFlightRe), &travel_field->flight_,
                 {log_manager, "kFlightRe"})) {
    // If any regex matches, then we found a travel field.
    return std::move(travel_field);
  }

  return nullptr;
}

void TravelField::AddClassifications(
    FieldCandidatesMap* field_candidates) const {
  // Simply tag all the fields as unknown types. Travel is currently used as
  // filter.
  AddClassification(passport_, UNKNOWN_TYPE, kBaseTravelParserScore,
                    field_candidates);
  AddClassification(origin_, UNKNOWN_TYPE, kBaseTravelParserScore,
                    field_candidates);
  AddClassification(destination_, UNKNOWN_TYPE, kBaseTravelParserScore,
                    field_candidates);
  AddClassification(flight_, UNKNOWN_TYPE, kBaseTravelParserScore,
                    field_candidates);
}

}  // namespace autofill
