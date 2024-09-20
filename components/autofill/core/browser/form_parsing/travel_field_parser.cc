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

namespace {
base::span<const MatchPatternRef> GetMatchPatterns(std::string_view name,
                                                   ParsingContext& context) {
  return GetMatchPatterns(name, context.page_language, context.pattern_file);
}
}  // namespace

TravelFieldParser::~TravelFieldParser() = default;

// static
std::unique_ptr<FormFieldParser> TravelFieldParser::Parse(
    ParsingContext& context,
    AutofillScanner* scanner) {
  if (!scanner || scanner->IsEnd()) {
    return nullptr;
  }

  base::span<const MatchPatternRef> passport_patterns =
      GetMatchPatterns("PASSPORT", context);
  base::span<const MatchPatternRef> travel_origin_patterns =
      GetMatchPatterns("TRAVEL_ORIGIN", context);
  base::span<const MatchPatternRef> travel_destination_patterns =
      GetMatchPatterns("TRAVEL_DESTINATION", context);
  base::span<const MatchPatternRef> flight_patterns =
      GetMatchPatterns("FLIGHT", context);

  auto travel_field = std::make_unique<TravelFieldParser>();
  if (ParseField(context, scanner, passport_patterns, &travel_field->passport_,
                 "PASSPORT") ||
      ParseField(context, scanner, travel_origin_patterns,
                 &travel_field->origin_, "TRAVEL_ORIGIN") ||
      ParseField(context, scanner, travel_destination_patterns,
                 &travel_field->destination_, "TRAVEL_DESTINATION") ||
      ParseField(context, scanner, flight_patterns, &travel_field->flight_,
                 "FLIGHT")) {
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
