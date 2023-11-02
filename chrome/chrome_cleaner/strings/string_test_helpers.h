// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_STRINGS_STRING_TEST_HELPERS_H_
#define CHROME_CHROME_CLEANER_STRINGS_STRING_TEST_HELPERS_H_

#include <string>
#include <vector>

namespace chrome_cleaner {

// Turns a string constant into a vector containing embedded nulls by
// converting every '0' to null.
std::vector<wchar_t> CreateVectorWithNulls(const std::wstring& str);

// Returns a wstring obtained from |v| by replacing null characters with "\\0".
std::wstring FormatVectorWithNulls(const std::vector<wchar_t>& v);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_STRINGS_STRING_TEST_HELPERS_H_
