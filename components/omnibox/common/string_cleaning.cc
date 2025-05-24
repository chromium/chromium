// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/common/string_cleaning.h"

#include <string>

#include "base/i18n/case_conversion.h"
#include "base/strings/escape.h"
#include "base/strings/utf_offset_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#include "url/gurl.h"

namespace string_cleaning {

namespace {
// The maximum length of URL or title returned by the Cleanup functions.
const size_t kCleanedUpUrlMaxLength = 1024u;
const size_t kCleanedUpTitleMaxLength = 1024u;
}  // namespace

// Attempts to shorten a URL safely (i.e., by preventing the end of the URL from
// being in the middle of an escape sequence) to no more than
// 'kCleanedUpUrlMaxLength' characters, returning the result.
std::string TruncateUrl(const std::string& url) {
  if (url.length() <= kCleanedUpUrlMaxLength) {
    return url;
  }

  // If we're in the middle of an escape sequence, truncate just before it.
  if (url[kCleanedUpUrlMaxLength - 1] == '%') {
    return url.substr(0, kCleanedUpUrlMaxLength - 1);
  }
  if (url[kCleanedUpUrlMaxLength - 2] == '%') {
    return url.substr(0, kCleanedUpUrlMaxLength - 2);
  }

  return url.substr(0, kCleanedUpUrlMaxLength);
}

std::u16string CleanUpUrlForMatching(
    const GURL& gurl,
    base::OffsetAdjuster::Adjustments* adjustments) {
  DCHECK(gurl.is_valid());

  base::OffsetAdjuster::Adjustments tmp_adjustments;
  return base::i18n::ToLower(url_formatter::FormatUrlWithAdjustments(
      GURL(TruncateUrl(gurl.spec())),
      url_formatter::kFormatUrlOmitUsernamePassword,
      base::UnescapeRule::SPACES | base::UnescapeRule::PATH_SEPARATORS |
          base::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS,
      nullptr, nullptr, adjustments ? adjustments : &tmp_adjustments));
}

std::u16string CleanUpTitleForMatching(const std::u16string& title) {
  return base::i18n::ToLower(title.substr(0u, kCleanedUpTitleMaxLength));
}

}  // namespace string_cleaning
