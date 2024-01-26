// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_CREDIT_CARD_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_CREDIT_CARD_FIELD_PARSER_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;

class CreditCardFieldParser : public FormFieldParser {
 public:
  explicit CreditCardFieldParser();

  CreditCardFieldParser(const CreditCardFieldParser&) = delete;
  CreditCardFieldParser& operator=(const CreditCardFieldParser&) = delete;

  ~CreditCardFieldParser() override;
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);

  // Instructions for how to format an expiration date for a text field.
  struct ExpirationDateFormat {
    // The expiration month is always assumed to be two digits and is therefore
    // not listed as a separate attribute.
    // The overall format is:
    // "{expiration month with 2 digits}{separator}{expiration year with
    // specified number of digits}"
    std::u16string separator;
    uint8_t digits_in_expiration_year = 0;
  };
  // Returns formatting instructions for an CC expiration date <input> field
  // based on properties of the field (maximum length, label, placeholder, ...).
  //
  // The `forced_field_type` is always used over detected patterns (like "MM /
  // YY" or "MM/YYYY") in a label or placeholder if possible. There are two main
  // usecases for this: 1) server overrides should be given precedence over
  // local heuristics. 2) During the final filling moment, we try to stick to
  // the overall type for filling and only diverge if the type does not fit into
  // the field. If the `forced_field_type` is `NO_SERVER_DATA`, it gets ignored.
  //
  // The `server_hint` is used if `DetermineExpirationDateFormat` does not
  // detect any pattern on the website. This is for classical crowdsourcing,
  // which sometimes makes errors. If the `server_hint` is `NO_SERVER_DATA`, it
  // gets ignored.
  //
  // The `fallback_type` specifies the number of digits to use for a field
  // if there are no hints on what's best. It must be either
  // CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR or CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR.
  static ExpirationDateFormat DetermineExpirationDateFormat(
      const AutofillField& field,
      FieldType fallback_type,
      FieldType server_hint,
      FieldType forced_field_type);

  // Returns the field type for an expiration year field in the following order
  // of priority: `forced_field_type` > type derived from heuristically
  // determined signals > `server_hint` > `fallback_type`. The server field
  // types can be UNKOWN_TYPE in which case they are ignored.
  static FieldType DetermineExpirationYearType(const AutofillField& field,
                                               FieldType fallback_type,
                                               FieldType server_hint,
                                               FieldType forced_field_type);

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  friend class CreditCardFieldParserTestBase;

  // Returns true if |scanner| points to a field that looks like a month
  // <select>.
  static bool LikelyCardMonthSelectField(AutofillScanner* scanner);

  // Returns true if |scanner| points to a field that looks like a year
  // <select> for a credit card. i.e. it contains the current year and
  // the next few years.
  static bool LikelyCardYearSelectField(ParsingContext* context,
                                        AutofillScanner* scanner);

  // Returns true if |scanner| points to a <select> field that contains credit
  // card type options.
  static bool LikelyCardTypeSelectField(AutofillScanner* scanner);

  // Returns true if |scanner| points to a field that is for a gift card number.
  // |scanner| advances if this returns true.
  // Prepaid debit cards do not count as gift cards, since they can be used like
  // a credit card.
  static bool IsGiftCardField(ParsingContext& context,
                              AutofillScanner* scanner);

  // Parses the expiration month/year/date fields. Returns true if it finds
  // something new.
  bool ParseExpirationDate(ParsingContext& context, AutofillScanner* scanner);

  // For the combined expiration field we return |exp_year_type_|; otherwise if
  // |expiration_year_| is having year with |max_length| of 2-digits we return
  // |CREDIT_CARD_EXP_2_DIGIT_YEAR|; otherwise |CREDIT_CARD_EXP_4_DIGIT_YEAR|.
  FieldType GetExpirationYearType() const;

  // Returns whether the expiration has been set for this credit card field.
  // It can be either a date or both the month and the year.
  bool HasExpiration() const;

  raw_ptr<AutofillField> cardholder_;  // Optional.

  // Occasionally pages have separate fields for the cardholder's first and
  // last names; for such pages |cardholder_| holds the first name field and
  // we store the last name field here.
  // (We could store an embedded |NameFieldParser| object here, but we don't do
  // so because the text patterns for matching a cardholder name are different
  // than for ordinary names, and because cardholder names never have titles,
  // middle names or suffixes.)
  raw_ptr<AutofillField> cardholder_last_;

  raw_ptr<AutofillField> type_;          // Optional.
  std::vector<raw_ptr<AutofillField, VectorExperimental>>
      numbers_;  // Required.

  // The 3-digit card verification number; we don't currently fill this.
  raw_ptr<AutofillField> verification_;

  // Either |expiration_date_| or both |expiration_month_| and
  // |expiration_year_| are required.
  raw_ptr<AutofillField> expiration_month_;
  raw_ptr<AutofillField> expiration_year_;
  raw_ptr<AutofillField> expiration_date_;

  // For combined expiration field having year as 2-digits we store here
  // |CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR|; otherwise we store
  // |CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR|.
  FieldType exp_year_type_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_CREDIT_CARD_FIELD_PARSER_H_
