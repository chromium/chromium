// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/autofill_parsing_utils.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;
class LogManager;

// This helper struct allows passing information into ParseField /
// ParseFieldSpecifics that can be used to create a log entry in
// chrome://autofill-internals explaining which regular expressions
// were matched by local heuristics.
struct RegExLogging {
  const raw_ptr<LogManager> log_manager = nullptr;
  const char* regex_name = "";
};

// Represents a logical form field in a web form.  Classes that implement this
// interface can identify themselves as a particular type of form field, e.g.
// name, phone number, or address field.
class FormField {
 public:
  FormField(const FormField&) = delete;
  FormField& operator=(const FormField&) = delete;

  virtual ~FormField() = default;

  // Classifies each field in |fields| with its heuristically detected type.
  // Each field has a derived unique name that is used as the key into the
  // returned FieldCandidatesMap.
  static FieldCandidatesMap ParseFormFields(
      const std::vector<std::unique_ptr<AutofillField>>& fields,
      const LanguageCode& page_language,
      bool is_form_tag,
      LogManager* log_manager = nullptr);

  // Looks for a promo code field in |fields|. Each field has a derived unique
  // name that is used as the key into the returned FieldCandidatesMap.
  static FieldCandidatesMap ParseFormFieldsForPromoCodes(
      const std::vector<std::unique_ptr<AutofillField>>& fields,
      const LanguageCode& page_language,
      bool is_form_tag,
      LogManager* log_manager = nullptr);

#if defined(UNIT_TEST)
  // Assign types to the fields for the testing purposes.
  void AddClassificationsForTesting(
      FieldCandidatesMap* field_candidates_for_testing) const {
    AddClassifications(field_candidates_for_testing);
  }
#endif

 protected:
  // Initial values assigned to FieldCandidates by their corresponding parsers.
  static const float kBaseEmailParserScore;
  static const float kBasePhoneParserScore;
  static const float kBaseTravelParserScore;
  static const float kBaseAddressParserScore;
  static const float kBaseCreditCardParserScore;
  static const float kBasePriceParserScore;
  static const float kBaseNameParserScore;
  static const float kBaseMerchantPromoCodeParserScore;
  static const float kBaseSearchParserScore;

  // Only derived classes may instantiate.
  FormField() = default;

  // Attempts to parse a form field with the given pattern.  Returns true on
  // success and fills |match| with a pointer to the field.
  static bool ParseField(AutofillScanner* scanner,
                         base::StringPiece16 pattern,
                         AutofillField** match,
                         const RegExLogging& logging = {});

  static bool ParseField(AutofillScanner* scanner,
                         const std::vector<MatchingPattern>& patterns,
                         AutofillField** match,
                         const RegExLogging& logging = {});

  static bool ParseField(AutofillScanner* scanner,
                         base::StringPiece16 pattern,
                         const std::vector<MatchingPattern>& patterns,
                         AutofillField** match,
                         const RegExLogging& logging = {});

  // Parses the stream of fields in |scanner| with regular expression |pattern|
  // as specified in the |match_type| bit field (see |MatchType|).  If |match|
  // is non-NULL and the pattern matches, |match| will be set to the matched
  // field, and the scanner would advance by one step. A |true| result is
  // returned in the case of a successful match, false otherwise.
  static bool ParseFieldSpecifics(AutofillScanner* scanner,
                                  base::StringPiece16 pattern,
                                  int match_type,
                                  AutofillField** match,
                                  const RegExLogging& logging = {});

  static bool ParseFieldSpecifics(AutofillScanner* scanner,
                                  const std::vector<MatchingPattern>& patterns,
                                  AutofillField** match,
                                  const RegExLogging& logging = {});

  // The same as ParseFieldSpecifics but with splitted match_types into
  // MatchAttributes and MatchFieldTypes.
  static bool ParseFieldSpecifics(AutofillScanner* scanner,
                                  base::StringPiece16 pattern,
                                  int match_field_attributes,
                                  int match_field_input_types,
                                  AutofillField** match,
                                  const RegExLogging& logging = {});

  struct MatchFieldBitmasks {
    int restrict_attributes = ~0;
    int augment_types = 0;
  };

  static bool ParseFieldSpecifics(AutofillScanner* scanner,
                                  base::StringPiece16 pattern,
                                  int match_type,
                                  const std::vector<MatchingPattern>& patterns,
                                  AutofillField** match,
                                  const RegExLogging& logging,
                                  MatchFieldBitmasks match_field_bitmasks = {
                                      .restrict_attributes = ~0,
                                      .augment_types = 0});

  // Attempts to parse a field with an empty label.  Returns true
  // on success and fills |match| with a pointer to the field.
  static bool ParseEmptyLabel(AutofillScanner* scanner, AutofillField** match);

  // Adds an association between a |field| and a |type| into |field_candidates|.
  // This association is weighted by |score|, the higher the stronger the
  // association.
  static void AddClassification(const AutofillField* field,
                                ServerFieldType type,
                                float score,
                                FieldCandidatesMap* field_candidates);

  // Returns true iff |type| matches |match_type|.
  static bool MatchesFormControlType(const std::string& type, int match_type);

  // Derived classes must implement this interface to supply field type
  // information.  |ParseFormFields| coordinates the parsing and extraction
  // of types from an input vector of |AutofillField| objects and delegates
  // the type extraction via this method.
  virtual void AddClassifications(
      FieldCandidatesMap* field_candidates) const = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(FormFieldTest, Match);
  FRIEND_TEST_ALL_PREFIXES(FormFieldTest, TestParseableLabels);

  // Function pointer type for the parsing function that should be passed to the
  // ParseFormFieldsPass() helper function.
  typedef std::unique_ptr<FormField> ParseFunction(
      AutofillScanner* scanner,
      const LanguageCode& page_language,
      LogManager* log_manager);

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
                              int match_type,
                              AutofillField** match,
                              const RegExLogging& logging = {});

  // The same as MatchAndAdvance but with splitted match_types into
  // MatchAttributes and MatchFieldTypes.
  static bool MatchAndAdvance(AutofillScanner* scanner,
                              base::StringPiece16 pattern,
                              int match_field_attributes,
                              int match_field_input_types,
                              AutofillField** match,
                              const RegExLogging& logging = {});

  // Matches the regular expression |pattern| against the components of
  // |field| as specified in the |match_type| bit field (see |MatchType|).
  static bool Match(const AutofillField* field,
                    base::StringPiece16 pattern,
                    int match_type,
                    const RegExLogging& logging = {});

  // The same as Match but with splitted match_types into MatchAttributes
  // and MatchFieldTypes.
  static bool Match(const AutofillField* field,
                    base::StringPiece16 pattern,
                    int match_field_attributes,
                    int match_field_input_types,
                    const RegExLogging& logging = {});

  // Perform a "pass" over the |fields| where each pass uses the supplied
  // |parse| method to match content to a given field type.
  // |fields| is both an input and an output parameter.  Upon exit |fields|
  // holds any remaining unclassified fields for further processing.
  // Classification results of the processed fields are stored in
  // |field_candidates|.
  static void ParseFormFieldsPass(ParseFunction parse,
                                  const std::vector<AutofillField*>& fields,
                                  FieldCandidatesMap* field_candidates,
                                  const LanguageCode& page_language,
                                  LogManager* log_manager = nullptr);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_H_
