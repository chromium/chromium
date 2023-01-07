// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/url_prefix.h"

#include "base/check_op.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace {

// Implements URLPrefix::BestURLPrefix(). Expects parameters to
// already be lowercased.
const URLPrefix* BestURLPrefixInternal(
    const std::u16string& lower_text,
    const std::u16string& lower_prefix_suffix) {
  const URLPrefixes& list = URLPrefix::GetURLPrefixes();
  for (const URLPrefix& prefix : list) {
    if (base::StartsWith(lower_text, prefix.prefix + lower_prefix_suffix,
                         base::CompareCase::SENSITIVE))
      return &prefix;
  }
  return nullptr;
}

// Like BestURLPrefixInternal() except also handles the prefix of "www.".
const URLPrefix* BestURLPrefixWithWWWCase(
    const std::u16string& lower_text,
    const std::u16string& lower_prefix_suffix) {
  static base::NoDestructor<URLPrefix> www_prefix(u"www.", 1);
  const URLPrefix* best_prefix =
      BestURLPrefixInternal(lower_text, lower_prefix_suffix);
  if ((best_prefix == nullptr ||
       best_prefix->num_components < www_prefix->num_components) &&
      base::StartsWith(lower_text, www_prefix->prefix + lower_prefix_suffix,
                       base::CompareCase::SENSITIVE))
    best_prefix = www_prefix.get();
  return best_prefix;
}

}  // namespace

URLPrefix::URLPrefix(const std::u16string& lower_prefix, size_t num_components)
    : prefix(lower_prefix), num_components(num_components) {
  // Input prefix must be in lowercase.
  DCHECK_EQ(lower_prefix, base::i18n::ToLower(lower_prefix));
}

// static
const URLPrefixes& URLPrefix::GetURLPrefixes() {
  static base::NoDestructor<URLPrefixes> prefixes([]() {
    URLPrefixes prefixes;

    // Keep this list in descending number of components.
    prefixes.push_back(URLPrefix(u"http://www.", 2));
    prefixes.push_back(URLPrefix(u"https://www.", 2));
    prefixes.push_back(URLPrefix(u"ftp://www.", 2));
    prefixes.push_back(URLPrefix(u"http://", 1));
    prefixes.push_back(URLPrefix(u"https://", 1));
    prefixes.push_back(URLPrefix(u"ftp://", 1));
    prefixes.push_back(URLPrefix(std::u16string(), 0));

    return prefixes;
  }());
  return *prefixes;
}

// static
const URLPrefix* URLPrefix::BestURLPrefix(const std::u16string& text,
                                          const std::u16string& prefix_suffix) {
  return BestURLPrefixInternal(base::i18n::ToLower(text),
                               base::i18n::ToLower(prefix_suffix));
}

// static
size_t URLPrefix::GetInlineAutocompleteOffset(
    const std::u16string& input,
    const std::u16string& fixed_up_input,
    const bool allow_www_prefix_without_scheme,
    const std::u16string& text) {
  const std::u16string lower_text(base::i18n::ToLower(text));
  const std::u16string lower_input(base::i18n::ToLower(input));
  const URLPrefix* best_prefix =
      allow_www_prefix_without_scheme
          ? BestURLPrefixWithWWWCase(lower_text, lower_input)
          : BestURLPrefixInternal(lower_text, lower_input);
  const std::u16string* matching_string = &input;
  // If we failed to find a best_prefix initially, try again using a fixed-up
  // version of the user input.  This is especially useful to get about: URLs
  // to inline against chrome:// shortcuts.  (about: URLs are fixed up to the
  // chrome:// scheme.)
  if (!best_prefix && !fixed_up_input.empty() && (fixed_up_input != input)) {
    const std::u16string lower_fixed_up_input(
        base::i18n::ToLower(fixed_up_input));
    best_prefix =
        allow_www_prefix_without_scheme
            ? BestURLPrefixWithWWWCase(lower_text, lower_fixed_up_input)
            : BestURLPrefixInternal(lower_text, lower_fixed_up_input);
    matching_string = &fixed_up_input;
  }
  return (best_prefix != nullptr)
             ? (best_prefix->prefix.length() + matching_string->length())
             : std::u16string::npos;
}
