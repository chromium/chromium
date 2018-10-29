// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_CORE_COMMON_LOCALE_UTIL_H_
#define COMPONENTS_LANGUAGE_CORE_COMMON_LOCALE_UTIL_H_

#include <string>
#include <vector>

namespace language {

// Split the |locale| into two parts. For example, if |locale| is 'en-US',
// this will be split into the main part 'en' and the tail part '-US'.
void SplitIntoMainAndTail(const std::string& locale,
                          std::string* main_part,
                          std::string* tail_part);

// Given a language code, extract the base language only.
// Example: from "en-US", extract "en".
std::string ExtractBaseLanguage(const std::string& language_code);

// Returns whether or not the given list includes at least one language with
// the same base as the input language.
// For example: "en-US" and "en-UK" share the same base "en".
bool ContainsSameBaseLanguage(const std::vector<std::string>& list,
                              const std::string& language_code);

// Converts |input_locale| to a fallback if needed and checks that the
// resulting locale is supported as a UI locale.
bool ConvertToFallbackUILocale(std::string* input_locale);

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
