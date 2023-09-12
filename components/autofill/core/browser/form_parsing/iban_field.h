// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_IBAN_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_IBAN_FIELD_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;
class LogManager;

// A form field that accepts International Bank Account Number (IBAN).
class IbanField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const GeoIpCountryCode& client_country,
      const LanguageCode& page_language,
      PatternSource pattern_source,
      LogManager* log_manager);

  explicit IbanField(const AutofillField* field);

  IbanField(const IbanField&) = delete;
  IbanField& operator=(const IbanField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  raw_ptr<const AutofillField> field_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_IBAN_FIELD_H_
