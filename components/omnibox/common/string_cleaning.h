// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_COMMON_STRING_CLEANING_H_
#define COMPONENTS_OMNIBOX_COMMON_STRING_CLEANING_H_

#include <string>

#include "base/strings/utf_offset_string_conversions.h"

class GURL;

namespace string_cleaning {

// Truncates an overly-long URL, unescapes it and interprets the characters as
// UTF-8 (both via `url_formatter::FormatUrl()`), and lower-cases it, returning
// the result. `adjustments`, if non-NULL, is set to reflect the transformations
// the URL spec underwent to become the return value.  If a caller computes
// offsets (e.g., for the position of matched text) in this cleaned-up string,
// it can use `adjustments` to calculate the location of these offsets in the
// original string (via `base::OffsetAdjuster::UnadjustOffsets()`).  This is
// useful if later the original string gets formatted in a different way for
// displaying. In this case, knowing the offsets in the original string will
// allow them to be properly translated to offsets in the newly-formatted
// string.
//
// The unescaping done by this function makes it possible to match substrings
// that were originally escaped for navigation; for example, if the user
// searched for "a&p", the query would be escaped as "a%26p", so without
// unescaping, an input string of "a&p" would no longer match this URL.  Note
// that the resulting unescaped URL may not be directly navigable (which is
// why it was escaped to begin with).
//
// `url` must be a valid URL.
std::u16string CleanUpUrlForMatching(
    const GURL& gurl,
    base::OffsetAdjuster::Adjustments* adjustments);

// Returns the lower-cased title, possibly truncated if the original title is
// overly-long.
std::u16string CleanUpTitleForMatching(const std::u16string& title);

}  // namespace string_cleaning

#endif  // COMPONENTS_OMNIBOX_COMMON_STRING_CLEANING_H_
