// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/travel_field_parser.h"

#include <memory>
#include <string_view>
#include <utility>

#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

TravelFieldParser::TravelFieldParser() = default;
TravelFieldParser::~TravelFieldParser() = default;

// static
std::unique_ptr<FormFieldParser> TravelFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner& scanner) {
  if (scanner.IsEnd()) {
    return nullptr;
  }

  auto travel_field = std::make_unique<TravelFieldParser>();
  if (ParseField(context, scanner, "PASSPORT", &travel_field->passport_) ||
      ParseField(context, scanner, "TRAVEL_ORIGIN", &travel_field->origin_) ||
      ParseField(context, scanner, "TRAVEL_DESTINATION",
                 &travel_field->destination_) ||
      ParseField(context, scanner, "FLIGHT", &travel_field->flight_)) {
    // If any regex matches, then we found a travel field.
    return std::move(travel_field);
  }

  return nullptr;
}

void TravelFieldParser::AddClassifications(
    FieldCandidatesMap& field_candidates) const {
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
