// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_PARSERS_PARSER_UTILS_COMMAND_LINE_ARGUMENTS_SANITIZER_H_
#define CHROME_CHROME_CLEANER_PARSERS_PARSER_UTILS_COMMAND_LINE_ARGUMENTS_SANITIZER_H_

#include <cstring>
#include <string>
#include <vector>

namespace chrome_cleaner {

// Receives a string of space separated command line arguments, sanitizes
// each one of them and returns them inside a vector.
std::vector<std::wstring> SanitizeArguments(const std::wstring& arguments);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_PARSERS_PARSER_UTILS_COMMAND_LINE_ARGUMENTS_SANITIZER_H_
