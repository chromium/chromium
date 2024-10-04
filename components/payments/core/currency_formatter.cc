// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/currency_formatter.h"

#include <memory>
#include <string_view>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/icu/source/common/unicode/stringpiece.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace payments {

namespace {

// Support a maximum of 10 fractional digits, similar to the ISO20022 standard.
// https://www.iso20022.org/standardsrepository/public/wqt/Description/mx/dico/
//   datatypes/_L8ZcEp0gEeOo48XfssNw8w
const int kMaximumNumFractionalDigits = 10;

// Max currency code length. Length of currency code can be at most 2048.
const static size_t kMaxCurrencyCodeLength = 2048;

// Currency codes longer than 6 characters get truncated to 5 + ellipsis.
const static size_t kMaxCurrencyCodeDisplayedChars = 6;

// Used to truncate long currency codes.
const char kEllipsis[] = "\xE2\x80\xA6";

// Returns whether the |currency_code| is valid to be used in ICU.
bool ShouldUseCurrencyCode(const std::string& currency_code) {
  return !currency_code.empty() &&
         currency_code.size() <= kMaxCurrencyCodeLength;
}

std::string FormatCurrencyCode(const std::string& currency_code) {
  return currency_code.length() < kMaxCurrencyCodeDisplayedChars
             ? currency_code
             : currency_code.substr(0, kMaxCurrencyCodeDisplayedChars - 1) +
                   kEllipsis;
}

}  // namespace

CurrencyFormatter::CurrencyFormatter(const std::string& currency_code,
                                     const std::string& locale_name)
    : locale_(locale_name.c_str()),
      formatted_currency_code_(FormatCurrencyCode(currency_code)) {
  UErrorCode error_code = U_ZERO_ERROR;
  icu_formatter_.reset(
      icu::NumberFormat::createCurrencyInstance(locale_, error_code));
  if (U_FAILURE(error_code)) {
    LOG(ERROR) << "Failed to initialize the currency formatter for "
               << locale_name;
    return;
  }

  if (ShouldUseCurrencyCode(currency_code)) {
    currency_code_ = std::make_unique<icu::UnicodeString>(
        currency_code.c_str(),
        base::checked_cast<int32_t>(currency_code.size()));
  } else {
    // For non-ISO4217 currency system/code, we use a dummy code which is not
    // going to appear in the output (stripped in Format()). This is because ICU
    // NumberFormat will not accept an empty currency code. Under these
    // circumstances, the number amount will be formatted according to locale,
    // which is desirable (e.g. "55.00" -> "55,00" in fr_FR).
    currency_code_ = std::make_unique<icu::UnicodeString>("DUM", 3);
  }

  icu_formatter_->setCurrency(currency_code_->getBuffer(), error_code);
  if (U_FAILURE(error_code)) {
    std::string currency_code_str;
    currency_code_->toUTF8String(currency_code_str);
    LOG(ERROR) << "Could not set currency code on currency formatter: "
               << currency_code_str;
    return;
  }

  icu_formatter_->setMaximumFractionDigits(kMaximumNumFractionalDigits);
}

CurrencyFormatter::~CurrencyFormatter() = default;

void CurrencyFormatter::SetMaxFractionalDigits(const int maxFractionalDigits) {
  icu_formatter_->setMaximumFractionDigits(maxFractionalDigits);
}

std::u16string CurrencyFormatter::Format(const std::string& amount) {
  // It's possible that the ICU formatter didn't initialize properly.
  if (!icu_formatter_ || !icu_formatter_->getCurrency())
    return base::UTF8ToUTF16(amount);

  icu::UnicodeString output;
  UErrorCode error_code = U_ZERO_ERROR;
  icu_formatter_->format(icu::StringPiece(amount.c_str()), output, nullptr,
                         error_code);

  if (output.isEmpty())
    return base::UTF8ToUTF16(amount);

  // Explicitly removes the currency code (truncated to its 3-letter, 2-letter
  // and 1-letter versions) from the output, because callers are expected to
  // display the currency code alongside this result.
  //
  // 3+ letters: If currency code is "ABCDEF" or "BTX", this code will
  // transform "ABC55.00"/"BTX55.00" to "55.00".
  // 2 letters: If currency code is "CAD", this code will transform "CA$55.00"
  // to "$55.00" (en_US) or "55,00 $ CA" to "55,00 $" (fr_FR).
  // 1 letter: If currency code is "AUD", this code will transform "A$55.00"
  // to "$55.00" (en_US).
  icu::UnicodeString tmp_currency_code(*currency_code_);
  tmp_currency_code.truncate(3);
  output.findAndReplace(tmp_currency_code, "");
  tmp_currency_code.truncate(2);
  output.findAndReplace(tmp_currency_code, "");
  tmp_currency_code.truncate(1);
  output.findAndReplace(tmp_currency_code, "");

  // In some locales, "-" sign comes before 3-letter currency code followed by
  // a space and the amount, removing currency code leaves a space between '-'
  // and the amount. e.g. In en-AU, -4.56 (USD) is formatted as '-USD 4.56'.
  // This change is a temporary work-around for updating ICU to 62.1/CLDR 33.1.
  // A rather peculiar requirement/behavior of CurrencyFormatter needs to be
  // reviewed. See  https://crbug.com/856113 .
  output.findAndReplace("- ", "-");
  output.findAndReplace(icu::UnicodeString::fromUTF8(u8"-\u00a0"), "-");

  // Trims any unicode whitespace (including non-breaking space).
  if (u_isUWhiteSpace(output[0])) {
    output.remove(0, 1);
  }
  if (u_isUWhiteSpace(output[output.length() - 1])) {
    output.remove(output.length() - 1, 1);
  }

  std::string output_str;
  output.toUTF8String(output_str);
  return base::UTF8ToUTF16(output_str);
}

}  // namespace payments
