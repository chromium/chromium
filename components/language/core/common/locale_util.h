// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_COMMON_LOCALE_UTIL_H_
#define COMPONENTS_LANGUAGE_CORE_COMMON_LOCALE_UTIL_H_

#include <string>
#include <string_view>
#include <utility>

namespace language {

// Split the |locale| into two parts. For example, if |locale| is 'en-US',
// this will be split into the main part 'en' and the tail part '-US'.
std::pair<std::string_view, std::string_view> SplitIntoMainAndTail(
    std::string_view locale);

// Given a language code, extract the base language only.
// Example: from "en-US", extract "en".
std::string_view ExtractBaseLanguage(std::string_view language_code);

// DEPRECATED. Use:
// - |l10n_util::CheckAndResolveLocale| to deterministically convert an input
//   locale to a UI locale. This matches the previous behaviour of
//   |ConvertToActualUILocale|, and is used internally in
//   |ConvertToActualUILocale|.
// - |l10n_util::IsUserFacingUILocale| to deterministically determine whether we
//   should show a user that a locale can be set as a UI locale.
// - |l10n_util::GetApplicationLocale| to get the application locale given an
//   input locale, with the correct fallbacks in case the provided locale is not
//   a UI locale. Note that this requires I/O, but can be modified to avoid I/O
//   when possible (by passing a perform_io argument to
//   |l10n_util::CheckAndResolveLocale| and |l10n_util::HasStringsForLocale|).
// Converts the input locale into its corresponding actual UI locale that
// Chrome should use for display and returns whether such locale exist.
// This method must be called whenever the display locale preference is
// read, because users can select a set of languages that is larger than
// the set of actual UI locales.
// If |input_locale| cannot be used as display UI, this method returns false
// and the content of |input_locale| is not modified.
bool ConvertToActualUILocale(std::string* input_locale);

}  // namespace language

#endif  // COMPONENTS_LANGUAGE_CORE_COMMON_LOCALE_UTIL_H_
