// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_CURRENCY_FORMATTER_H_
#define COMPONENTS_PAYMENTS_CORE_CURRENCY_FORMATTER_H_

#include <memory>
#include <string>

#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/numfmt.h"

namespace payments {

// Currency formatter for amounts, according to a currency code, which typically
// adheres to [ISO4217] (for example, "USD" for US Dollars).
class CurrencyFormatter {
 public:
  // Initializes the CurrencyFormatter for a given |currency_code|,
  // |locale_name|. Note that |currency_code| should have been validated
  // (as part of payment_details_validation.h) before this is created.
  CurrencyFormatter(const std::string& currency_code,
                    const std::string& locale_name);

  CurrencyFormatter(const CurrencyFormatter&) = delete;
  CurrencyFormatter& operator=(const CurrencyFormatter&) = delete;

  ~CurrencyFormatter();

  // Set the maximum number of fractional digits. (kMaximumNumFractionalDigits
  // is the default if unset)
  void SetMaxFractionalDigits(const int maxFractionalDigits);

  // Formats the |amount| according to the currency code that was set. The
  // result will NOT contain the currency code, nor a subset of it. Rather, the
  // caller of this function should display the currency code separately. The
  // return value may contain non-breaking space and is ready for display. In
  // the case of a failure in initialization of the formatter or during
  // formatter, this method will return |amount|.
  std::u16string Format(const std::string& amount);

  // Returns the formatted currency code (<= 6 characters including ellipsis if
  // applicable).
  const std::string& formatted_currency_code() const {
    return formatted_currency_code_;
  }

 private:
  const icu::Locale locale_;
  std::unique_ptr<icu::UnicodeString> currency_code_;
  std::string formatted_currency_code_;
  std::unique_ptr<icu::NumberFormat> icu_formatter_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_CURRENCY_FORMATTER_H_
