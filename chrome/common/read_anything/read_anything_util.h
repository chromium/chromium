// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_READ_ANYTHING_READ_ANYTHING_UTIL_H_
#define CHROME_COMMON_READ_ANYTHING_READ_ANYTHING_UTIL_H_

#include <string>
#include <string_view>
#include <vector>

// Returns font names, in order, that should be shown to the user as choices
// based on the provided `language_code`. If `language_code` is empty, returns
// all fonts.
[[nodiscard]] std::vector<std::string> GetSupportedFonts(
    std::string_view language_code);

// Records `font_name` in histograms.
void LogFontName(std::string_view font_name);

#endif  // CHROME_COMMON_READ_ANYTHING_READ_ANYTHING_UTIL_H_
