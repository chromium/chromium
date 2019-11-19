// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_CREDIT_CARD_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_CREDIT_CARD_FIELD_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"

namespace autofill {

class AutofillField;
class AutofillScanner;
class LogManager;

class CreditCardField : public FormField {
 public:
  explicit CreditCardField(LogManager* log_manager);
  ~CreditCardField() override;
  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          LogManager* log_manager);

 protected:
  void AddClassifications(FieldCandidatesMap* field_candidates) const override;

 private:
  friend class CreditCardFieldTestBase;

  // Returns true if |scanner| points to a field that looks like a month
  // <select>.
  static bool LikelyCardMonthSelectField(AutofillScanner* scanner);

  // Returns true if |scanner| points to a field that looks like a year
  // <select> for a credit card. i.e. it contains the current year and
  // the next few years.
  static bool LikelyCardYearSelectField(AutofillScanner* scanner);

  // Returns true if |scanner| points to a <select> field that contains credit
  // card type options.
  static bool LikelyCardTypeSelectField(AutofillScanner* scanner);

  // Returns true if |scanner| points to a field that is for a gift card number.
  // |scanner| advances if this returns true.
  // Prepaid debit cards do not count as gift cards, since they can be used like
  // a credit card.
  static bool IsGiftCardField(AutofillScanner* scanner,
                              LogManager* log_manager);

  // Parses the expiration month/year/date fields. Returns true if it finds
  // something new.
  bool ParseExpirationDate(AutofillScanner* scanner);

  // For the combined expiration field we return |exp_year_type_|; otherwise if
  // |expiration_year_| is having year with |max_length| of 2-digits we return
  // |CREDIT_CARD_EXP_2_DIGIT_YEAR|; otherwise |CREDIT_CARD_EXP_4_DIGIT_YEAR|.
  ServerFieldType GetExpirationYearType() const;

  // Returns whether the expiration has been set for this credit card field.
  // It can be either a date or both the month and the year.
  bool HasExpiration() const;

  LogManager* log_manager_;  // Optional.

  AutofillField* cardholder_;  // Optional.

  // Occasionally pages have separate fields for the cardholder's first and
  // last names; for such pages |cardholder_| holds the first name field and
  // we store the last name field here.
  // (We could store an embedded |NameField| object here, but we don't do so
  // because the text patterns for matching a cardholder name are different
  // than for ordinary names, and because cardholder names never have titles,
  // middle names or suffixes.)
  AutofillField* cardholder_last_;

  AutofillField* type_;                  // Optional.
  std::vector<AutofillField*> numbers_;  // Required.

  // The 3-digit card verification number; we don't currently fill this.
  AutofillField* verification_;

  // Either |expiration_date_| or both |expiration_month_| and
  // |expiration_year_| are required.
  AutofillField* expiration_month_;
  AutofillField* expiration_year_;
  AutofillField* expiration_date_;

  // For combined expiration field having year as 2-digits we store here
  // |CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR|; otherwise we store
  // |CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR|.
  ServerFieldType exp_year_type_;

  DISALLOW_COPY_AND_ASSIGN(CreditCardField);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_CREDIT_CARD_FIELD_H_
