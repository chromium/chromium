// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/browser/form_parsing/regex_patterns.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;
class LogManager;

// This helper struct allows passing information into ParseField() and
// ParseFieldSpecifics() that can be used to create a log entry in
// chrome://autofill-internals explaining which regular expressions
// were matched by local heuristics.
struct RegExLogging {
  const raw_ptr<LogManager> log_manager = nullptr;
  const char* regex_name = "";
};

// Represents a logical form field in a web form. Classes that implement this
// interface can identify themselves as a particular type of form field, e.g.
// name, phone number, or address field.
class FormField {
 public:
  FormField(const FormField&) = delete;
  FormField& operator=(const FormField&) = delete;

  virtual ~FormField() = default;

  // Classifies each field in |fields| with its heuristically detected type.
  // Each field has a derived unique name that is used as the key into
  // |field_candidates|.
  static void ParseFormFields(
      const std::vector<std::unique_ptr<AutofillField>>& fields,
      const GeoIpCountryCode& client_country,
      const LanguageCode& page_language,
      bool is_form_tag,
      PatternSource pattern_source,
      FieldCandidatesMap& field_candidates,
      LogManager* log_manager = nullptr);

  // Looks for types that are allowed to appear in solitary (such as merchant
  // promo codes) inside |fields|. Each field has a derived unique name that is
  // used as the key into |field_candidates|.
  static void ParseSingleFieldForms(
      const std::vector<std::unique_ptr<AutofillField>>& fields,
      const GeoIpCountryCode& client_country,
      const LanguageCode& page_language,
      bool is_form_tag,
      PatternSource pattern_source,
      FieldCandidatesMap& field_candidates,
      LogManager* log_manager = nullptr);

  // Search for standalone CVC fields inside `fields`. Standalone CVC fields
  // are CVC fields that should appear without any credit card field or email
  // address in the same form. Each field has a derived unique name that is
  // used as the key into `field_candidates`. Standalone CVC fields have unique
  // prerequisites in that there shouldn't be other credit card or email fields
  // in the form, which is why its parsing logic is extracted to its own method.
  static void ParseStandaloneCVCFields(
      const std::vector<std::unique_ptr<AutofillField>>& fields,
      const LanguageCode& page_language,
      PatternSource pattern_source,
      FieldCandidatesMap& field_candidates,
      LogManager* log_manager = nullptr);

#if defined(UNIT_TEST)
  static bool MatchForTesting(const AutofillField* field,
                              base::StringPiece16 pattern,
                              MatchParams match_type,
                              const RegExLogging& logging = {}) {
    return FormField::Match(field, pattern, match_type, logging);
  }

  static bool ParseInAnyOrderForTesting(
      AutofillScanner* scanner,
      std::vector<
          std::pair<raw_ptr<AutofillField>*, base::RepeatingCallback<bool()>>>
          fields_and_parsers) {
    return FormField::ParseInAnyOrder(scanner, fields_and_parsers);
  }

  // Assign types to the fields for the testing purposes.
  void AddClassificationsForTesting(
      FieldCandidatesMap& field_candidates_for_testing) const {
    AddClassifications(field_candidates_for_testing);
  }
#endif

 protected:
  // Initial values assigned to FieldCandidates by their corresponding parsers.
  // There's an implicit precedence determined by the values assigned here.
  // Email is currently the most important followed by Phone, Travel, Address,
  // Birthdate, Credit Card, IBAN, Price, Name, Merchant promo code, and Search.
  static constexpr float kBaseEmailParserScore = 1.4f;
  static constexpr float kBasePhoneParserScore = 1.3f;
  static constexpr float kBaseTravelParserScore = 1.2f;
  static constexpr float kBaseAddressParserScore = 1.1f;
  static constexpr float kBaseBirthdateParserScore = 1.05f;
  static constexpr float kBaseCreditCardParserScore = 1.0f;
  static constexpr float kBaseIbanParserScore = 0.975f;
  static constexpr float kBasePriceParserScore = 0.95f;
  static constexpr float kBaseNameParserScore = 0.9f;
  static constexpr float kBaseMerchantPromoCodeParserScore = 0.85f;
  static constexpr float kBaseSearchParserScore = 0.8f;
  static constexpr float kBaseNumericQuantityParserScore = 0.75f;
  static constexpr float kBaseAutocompleteParserScore = 0.05f;

  // Only derived classes may instantiate.
  FormField() = default;

  // Calls MatchesRegex() with a thread-safe cache.
  // Should not be called from the UI thread as it may be blocked on a worker
  // thread.
  static bool MatchesRegexWithCache(
      base::StringPiece16 input,
      base::StringPiece16 pattern,
      std::vector<std::u16string>* groups = nullptr);

  // Attempts to parse a form field with the given pattern.  Returns true on
  // success and fills |match| with a pointer to the field.
  static bool ParseField(AutofillScanner* scanner,
                         base::StringPiece16 pattern,
                         base::span<const MatchPatternRef> patterns,
                         raw_ptr<AutofillField>* match,
                         const RegExLogging& logging = {});

  // TODO(crbug/1142936): Remove `projection` if it's not needed anymore.
  static bool ParseFieldSpecifics(
      AutofillScanner* scanner,
      base::StringPiece16 pattern,
      const MatchParams& match_type,
      base::span<const MatchPatternRef> patterns,
      raw_ptr<AutofillField>* match,
      const RegExLogging& logging,
      MatchingPattern (*projection)(const MatchingPattern&) = nullptr);

  // Attempts to parse a field with an empty label. Returns true
  // on success and fills |match| with a pointer to the field.
  static bool ParseEmptyLabel(AutofillScanner* scanner,
                              raw_ptr<AutofillField>* match);

  // Attempts to parse several fields using the specified parsing functions in
  // arbitrary order. This is useful e.g. when parsing dates, where both dd/mm
  // and mm/dd makes sense.
  // Returns true if all fields were parsed successfully. In this case, the
  // fields are assigned with the matching ones.
  // If no order is matched every parser, false is returned, all fields are
  // reset to nullptr and the scanner is rewound to it's original position.
  static bool ParseInAnyOrder(
      AutofillScanner* scanner,
      std::vector<
          std::pair<raw_ptr<AutofillField>*, base::RepeatingCallback<bool()>>>
          fields_and_parsers);

  // Adds an association between a |field| and a |type| into |field_candidates|.
  // This association is weighted by |score|, the higher the stronger the
  // association.
  static void AddClassification(const AutofillField* field,
                                ServerFieldType type,
                                float score,
                                FieldCandidatesMap& field_candidates);

  // Returns true iff |type| matches |match_type|.
  static bool MatchesFormControlType(base::StringPiece type,
                                     DenseSet<MatchFieldType> match_type);

  // TODO(crbug.com/1352826) Undo making this temporarily a public function.
 public:
  // Returns true if |field_type| is a single field parseable type.
  static bool IsSingleFieldParseableType(ServerFieldType field_type);

 protected:
  // Derived classes must implement this interface to supply field type
  // information.  |ParseFormFields| coordinates the parsing and extraction
  // of types from an input vector of |AutofillField| objects and delegates
  // the type extraction via this method.
  virtual void AddClassifications(
      FieldCandidatesMap& field_candidates) const = 0;

 private:
  // Function pointer type for the parsing function that should be passed to the
  // ParseFormFieldsPass() helper function.
  typedef std::unique_ptr<FormField> ParseFunction(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      PatternSource pattern_source,
      LogManager* log_manager);

  // Removes entries from `field_candidates` in case
  // - not enough fields were classified by local heuristics.
  // - fields were not explicitly allow-listed because they appear in
  //   contexts that don't contain enough fields (e.g. forms with only an
  //   email address).
  static void ClearCandidatesIfHeuristicsDidNotFindEnoughFields(
      const std::vector<std::unique_ptr<AutofillField>>& fields,
      FieldCandidatesMap& field_candidates,
      bool is_form_tag,
      const GeoIpCountryCode& client_country,
      LogManager* log_manager);

  static bool ParseFieldSpecificsWithNewPatterns(
      AutofillScanner* scanner,
      base::span<const MatchPatternRef> patterns,
      raw_ptr<AutofillField>* match,
      const RegExLogging& logging,
      MatchingPattern (*projection)(const MatchingPattern&));

  // Parses the stream of fields in |scanner| with regular expression |pattern|
  // as specified in |match_type|. If |match| is non-NULL and the pattern
  // matches, |match| will be set to the matched field, and the scanner would
  // advance by one step. A |true| result is returned in the case of a
  // successful match, false otherwise.
  static bool ParseFieldSpecificsWithLegacyPattern(
      AutofillScanner* scanner,
      base::StringPiece16 pattern,
      MatchParams match_type,
      raw_ptr<AutofillField>* match,
      const RegExLogging& logging);

  // Removes checkable fields and returns fields to be processed for field
  // detection.
  static std::vector<AutofillField*> RemoveCheckableFields(
      const std::vector<std::unique_ptr<AutofillField>>& fields);

  // Matches |pattern| to the contents of the field at the head of the
  // |scanner|.
  // Returns |true| if a match is found according to |match_type|, and |false|
  // otherwise.
  static bool MatchAndAdvance(AutofillScanner* scanner,
                              base::StringPiece16 pattern,
                              MatchParams match_type,
                              raw_ptr<AutofillField>* match,
                              const RegExLogging& logging = {});

  // Matches the regular expression |pattern| against the components of
  // |field| as specified in |match_type|.
  static bool Match(const AutofillField* field,
                    base::StringPiece16 pattern,
                    MatchParams match_type,
                    const RegExLogging& logging = {});

  // Perform a "pass" over the |fields| where each pass uses the supplied
  // |parse| method to match content to a given field type.
  // |fields| is both an input and an output parameter.  Upon exit |fields|
  // holds any remaining unclassified fields for further processing.
  // Classification results of the processed fields are stored in
  // |field_candidates|.
  static void ParseFormFieldsPass(ParseFunction parse,
                                  const std::vector<AutofillField*>& fields,
                                  FieldCandidatesMap& field_candidates,
                                  const LanguageCode& page_language,
                                  PatternSource pattern_source,
                                  LogManager* log_manager);

  // Interpret the fields' `parsable_name()` (id or name attribute) as an
  // autocomplete type and classify them by it. E.g. <input id=given-name>.
  static void ParseUsingAutocompleteAttributes(
      const std::vector<AutofillField*>& fields,
      FieldCandidatesMap& field_candidates);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_H_
