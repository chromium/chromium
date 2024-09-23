// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_PARSER_NG_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_PARSER_NG_H_

#include <map>
#include <memory>
#include <string_view>

#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"

namespace autofill {

// A partial or final interpretation of fields.
// A `ClassifiedFieldSequence` assigns field types to fields such that
// - no field type can be assigned to two different fields.
// - no field can be assigned more than one type.
struct ClassifiedFieldSequence {
  ClassifiedFieldSequence();
  ~ClassifiedFieldSequence();

  // A `ClassifiedFieldSequence` is better than another if it contains more
  // classified fields. In case of an equal number of classified fields,
  // the `score` is used as a tiebreaker.
  bool BetterThan(const ClassifiedFieldSequence& other) const;

  base::flat_map<FieldType, raw_ptr<AutofillField>> assignments;
  // The set of field types that exist in `assignments`. As a performance
  // optimization, we don't delete entries from `assignments` (flat_map is slow
  // when the keyset is modified) but write null entries instead.
  // `contained_types` is a quick way to check whether a value is currently
  // assigned in `assignments`.
  FieldTypeSet contained_types;
  // The index of the last field in the form which was assigned a field type.
  size_t last_classified_field_index = 0u;
  // The score of a classification. Bigger is better. Tiebreaker in case two
  // `ClassifiedFieldSequence`s have the same number of classified fields.
  double score = 0.0;
};

// This is an address field parser that is based on backtracing to find the
// highest scoring set of assignments of types to fields while respecting a set
// of constraints (e.g. which field types may co-occur, the order in which field
// types may occur, ...).
class AddressFieldParserNG : public FormFieldParser {
 public:
  // This class stores precalculated work for the address hierarchy of a
  // specific country which should be precalculated to prevent repetitive work
  // during form parsing.
  class FieldTypeInformation;

  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);

  ~AddressFieldParserNG() override;
  AddressFieldParserNG(const AddressFieldParserNG&) = delete;
  AddressFieldParserNG& operator=(const AddressFieldParserNG&) = delete;

 protected:
  // Override of FormFieldParser.
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  explicit AddressFieldParserNG(AddressCountryCode country_code);

  // Wrapper for ::autofill::GetMatchPatterns which considers the current
  // page language and pattern source from the `context_`.
  base::span<const MatchPatternRef> GetMatchPatterns(std::string_view name);

  // Returns the score of the best matching rule that assigns `field_type` to
  // the field at the current cursor position. If no rule matches, std::nullopt
  // is returned.
  //
  // The scores don't correspond to the scores in `autofill::MatchingPattern`
  // to enable a contextual scoring. E.g. the "ADDRESS_LINE_1" patterns are used
  // for ADDRESS_HOME_STREET_ADDRESS and ADDRESS_HOME_LINE1 but matches are
  // assigned different scores.
  std::optional<double> FindScoreOfBestMatchingRule(FieldType field_type);

  // Function to block certain combinations of field types that cannot be
  // expressed by the type-incompatibility detection mechanism of
  // `FieldTypeInformation`. E.g. we should not assign ADDRESS_HOME_HOUSE_NUMBER
  // to a field unless we have more evidence that the field is part of a
  // structured address form.
  bool IsClassificationPlausible() const;

  // The core backtracking algorithm.
  //
  // For the field at the current cursor position it tries to assign all
  // possible field types and proceed recursively on the next field. If all
  // fields have a type assigned or no new field type can be assigned to the
  // current cursor position, the current classification is considered for being
  // a new best classification.
  //
  // The following invariant is maintained: After a call of `ParseRecursively`
  // returns, `partial_classification_` and `scanner_` must be in the same state
  // as before (`partial_classification_` must contain the same assignments and
  // `scanner_` put point to the same field). The `best_classification_` may be
  // updated.
  void ParseRecursively();

  // Prepared work for the address hierarchy corresponding to the country of the
  // client.
  std::unique_ptr<FieldTypeInformation> field_types_;

  // The best classification encountered so far.
  ClassifiedFieldSequence best_classification_;

  // The following variables are only set and valid while a `Parse()` operation
  // is in progress and must not be accessed afterwards:
  raw_ptr<ParsingContext> context_;
  raw_ptr<AutofillScanner> scanner_;
  raw_ptr<AutofillField> initial_field_;

  // An intermediate classification (assignment of field types to fields) that
  // is being worked on during the parsing.
  ClassifiedFieldSequence partial_classification_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_ADDRESS_FIELD_PARSER_NG_H_
