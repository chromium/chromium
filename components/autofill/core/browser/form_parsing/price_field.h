// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PRICE_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PRICE_FIELD_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;
class LogManager;

// Price fields are not filled by autofill, but identifying them will help to
// reduce the number of false positives.
class PriceField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          const LanguageCode& page_language,
                                          PatternSource pattern_source,
                                          LogManager* log_manager);
  explicit PriceField(const AutofillField* field);

  PriceField(const PriceField&) = delete;
  PriceField& operator=(const PriceField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(PriceFieldTest, ParsePrice);
  FRIEND_TEST_ALL_PREFIXES(PriceFieldTest, ParseNonPrice);

  raw_ptr<const AutofillField> field_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PRICE_FIELD_H_
