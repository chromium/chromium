// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/travel_field.h"

#include <memory>
#include <utility>

#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/autofill_regex_constants.h"

namespace autofill {

TravelField::~TravelField() = default;

// static
std::unique_ptr<FormField> TravelField::Parse(AutofillScanner* scanner,
                                              const LanguageCode& page_language,
                                              PatternSource pattern_source,
                                              LogManager* log_manager) {
  if (!scanner || scanner->IsEnd())
    return nullptr;

  base::span<const MatchPatternRef> passport_patterns =
      GetMatchPatterns("PASSPORT", page_language, pattern_source);
  base::span<const MatchPatternRef> travel_origin_patterns =
      GetMatchPatterns("TRAVEL_ORIGIN", page_language, pattern_source);
  base::span<const MatchPatternRef> travel_destination_patterns =
      GetMatchPatterns("TRAVEL_DESTINATION", page_language, pattern_source);
  base::span<const MatchPatternRef> flight_patterns =
      GetMatchPatterns("FLIGHT", page_language, pattern_source);

  auto travel_field = std::make_unique<TravelField>();
  if (ParseField(scanner, kPassportRe, passport_patterns,
                 &travel_field->passport_, {log_manager, "kPassportRe"}) ||
      ParseField(scanner, kTravelOriginRe, travel_origin_patterns,
                 &travel_field->origin_, {log_manager, "kTravelOriginRe"}) ||
      ParseField(scanner, kTravelDestinationRe, travel_destination_patterns,
                 &travel_field->destination_,
                 {log_manager, "kTravelDestinationRe"}) ||
      ParseField(scanner, kFlightRe, flight_patterns, &travel_field->flight_,
                 {log_manager, "kFlightRe"})) {
    // If any regex matches, then we found a travel field.
    return std::move(travel_field);
  }

  return nullptr;
}

void TravelField::AddClassifications(
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
