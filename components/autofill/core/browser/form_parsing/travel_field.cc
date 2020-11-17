// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_parsing/travel_field.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_regex_constants.h"

namespace autofill {

TravelField::~TravelField() = default;

// static
std::unique_ptr<FormField> TravelField::Parse(AutofillScanner* scanner,
                                              const LanguageCode& page_language,
                                              LogManager* log_manager) {
  if (!scanner || scanner->IsEnd())
    return nullptr;

  const std::vector<MatchingPattern>& passport_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("PASSPORT",
                                                      page_language);
  const std::vector<MatchingPattern>& travel_origin_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("TRAVEL_ORIGIN",
                                                      page_language);
  const std::vector<MatchingPattern>& travel_destination_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("TRAVEL_DESTINATION",
                                                      page_language);
  const std::vector<MatchingPattern>& flight_patterns =
      PatternProvider::GetInstance().GetMatchPatterns("FLIGHT", page_language);

  auto travel_field = std::make_unique<TravelField>();
  if (ParseField(scanner, base::UTF8ToUTF16(kPassportRe), passport_patterns,
                 &travel_field->passport_, {log_manager, "kPassportRe"}) ||
      ParseField(scanner, base::UTF8ToUTF16(kTravelOriginRe),
                 travel_origin_patterns, &travel_field->origin_,
                 {log_manager, "kTravelOriginRe"}) ||
      ParseField(scanner, base::UTF8ToUTF16(kTravelDestinationRe),
                 travel_destination_patterns, &travel_field->destination_,
                 {log_manager, "kTravelDestinationRe"}) ||
      ParseField(scanner, base::UTF8ToUTF16(kFlightRe), flight_patterns,
                 &travel_field->flight_, {log_manager, "kFlightRe"})) {
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
