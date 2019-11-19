// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_H_

#include <memory>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/field_candidates.h"

namespace autofill {

class AutofillField;
class AutofillScanner;
class LogManager;

// This helper struct allows passing information into ParseField /
// ParseFieldSpecifics that can be used to create a log entry in
// chrome://autofill-internals explaining which regular expressions
// were matched by local heuristics.
struct RegExLogging {
  LogManager* const log_manager = nullptr;
  const char* regex_name = "";
};

// Represents a logical form field in a web form.  Classes that implement this
// interface can identify themselves as a particular type of form field, e.g.
// name, phone number, or address field.
class FormField {
 public:
  virtual ~FormField() {}

  // Classifies each field in |fields| with its heuristically detected type.
  // Each field has a derived unique name that is used as the key into the
  // returned FieldCandidatesMap.
  static FieldCandidatesMap ParseFormFields(
      const std::vector<std::unique_ptr<AutofillField>>& fields,
      bool is_form_tag,
      LogManager* log_manager = nullptr);

 protected:
  // A bit-field used for matching specific parts of a field in question.
  enum MatchType {
    // Attributes.
    MATCH_LABEL = 1 << 0,
    MATCH_NAME = 1 << 1,

    // Input types.
    MATCH_TEXT = 1 << 2,
    MATCH_EMAIL = 1 << 3,
    MATCH_TELEPHONE = 1 << 4,
    MATCH_SELECT = 1 << 5,
    MATCH_TEXT_AREA = 1 << 6,
    MATCH_PASSWORD = 1 << 7,
    MATCH_NUMBER = 1 << 8,
    MATCH_SEARCH = 1 << 9,
    MATCH_ALL_INPUTS = MATCH_TEXT | MATCH_EMAIL | MATCH_TELEPHONE |
                       MATCH_SELECT | MATCH_TEXT_AREA | MATCH_PASSWORD |
                       MATCH_NUMBER | MATCH_SEARCH,

    // By default match label and name for input/text types.
    MATCH_DEFAULT = MATCH_LABEL | MATCH_NAME | MATCH_TEXT,
  };

  // Initial values assigned to FieldCandidates by their corresponding parsers.
  static const float kBaseEmailParserScore;
  static const float kBasePhoneParserScore;
  static const float kBaseTravelParserScore;
  static const float kBaseAddressParserScore;
  static const float kBaseCreditCardParserScore;
  static const float kBasePriceParserScore;
  static const float kBaseNameParserScore;
  static const float kBaseSearchParserScore;

  // Only derived classes may instantiate.
  FormField() {}

  // Attempts to parse a form field with the given pattern.  Returns true on
  // success and fills |match| with a pointer to the field.
  static bool ParseField(AutofillScanner* scanner,
                         const base::string16& pattern,
                         AutofillField** match,
                         const RegExLogging& logging = {});

  // Parses the stream of fields in |scanner| with regular expression |pattern|
  // as specified in the |match_type| bit field (see |MatchType|).  If |match|
  // is non-NULL and the pattern matches, |match| will be set to the matched
  // field, and the scanner would advance by one step. A |true| result is
  // returned in the case of a successful match, false otherwise.
  static bool ParseFieldSpecifics(AutofillScanner* scanner,
                                  const base::string16& pattern,
                                  int match_type,
                                  AutofillField** match,
                                  const RegExLogging& logging = {});

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

  // Function pointer type for the parsing function that should be passed to the
  // ParseFormFieldsPass() helper function.
  typedef std::unique_ptr<FormField> ParseFunction(AutofillScanner* scanner,
                                                   LogManager* log_manager);

  // Matches |pattern| to the contents of the field at the head of the
  // |scanner|.
  // Returns |true| if a match is found according to |match_type|, and |false|
  // otherwise.
  static bool MatchAndAdvance(AutofillScanner* scanner,
                              const base::string16& pattern,
                              int match_type,
                              AutofillField** match,
                              const RegExLogging& logging = {});

  // Matches the regular expression |pattern| against the components of |field|
  // as specified in the |match_type| bit field (see |MatchType|).
  static bool Match(const AutofillField* field,
                    const base::string16& pattern,
                    int match_type,
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
                                  LogManager* log_manager = nullptr);

  DISALLOW_COPY_AND_ASSIGN(FormField);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_FORM_FIELD_H_
