// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_MERCHANT_PROMO_CODE_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_MERCHANT_PROMO_CODE_FIELD_H_

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

// A form field that accepts promo/gift/coupon codes during checkout on a
// merchant's web site.
class MerchantPromoCodeField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          const LanguageCode& page_language,
                                          PatternSource pattern_source,
                                          LogManager* log_manager);
  explicit MerchantPromoCodeField(const AutofillField* field);

  MerchantPromoCodeField(const MerchantPromoCodeField&) = delete;
  MerchantPromoCodeField& operator=(const MerchantPromoCodeField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MerchantPromoCodeFieldTest, ParsePromoCode);
  FRIEND_TEST_ALL_PREFIXES(MerchantPromoCodeFieldTest, ParseCouponCode);
  FRIEND_TEST_ALL_PREFIXES(MerchantPromoCodeFieldTest, ParseGiftCode);
  FRIEND_TEST_ALL_PREFIXES(MerchantPromoCodeFieldTest, ParseNonPromoCode);
  FRIEND_TEST_ALL_PREFIXES(MerchantPromoCodeFieldTest, ParsePromoCodeFlagOff);

  raw_ptr<const AutofillField> field_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_MERCHANT_PROMO_CODE_FIELD_H_
