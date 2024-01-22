// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_MERCHANT_PROMO_CODE_FIELD_PARSER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_MERCHANT_PROMO_CODE_FIELD_PARSER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/form_parsing/form_field_parser.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;

// A form field that accepts promo/gift/coupon codes during checkout on a
// merchant's web site.
class MerchantPromoCodeFieldParser : public FormFieldParser {
 public:
  static std::unique_ptr<FormFieldParser> Parse(ParsingContext& context,
                                                AutofillScanner* scanner);
  explicit MerchantPromoCodeFieldParser(const AutofillField* field);

  MerchantPromoCodeFieldParser(const MerchantPromoCodeFieldParser&) = delete;
  MerchantPromoCodeFieldParser& operator=(const MerchantPromoCodeFieldParser&) =
      delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MerchantPromoCodeFieldParserTest, ParsePromoCode);
  FRIEND_TEST_ALL_PREFIXES(MerchantPromoCodeFieldParserTest, ParseCouponCode);
  FRIEND_TEST_ALL_PREFIXES(MerchantPromoCodeFieldParserTest, ParseGiftCode);
  FRIEND_TEST_ALL_PREFIXES(MerchantPromoCodeFieldParserTest, ParseNonPromoCode);
  FRIEND_TEST_ALL_PREFIXES(MerchantPromoCodeFieldParserTest,
                           ParsePromoCodeFlagOff);

  raw_ptr<const AutofillField> field_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_MERCHANT_PROMO_CODE_FIELD_PARSER_H_
