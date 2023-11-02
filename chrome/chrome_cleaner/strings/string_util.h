// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_STRINGS_STRING_UTIL_H_
#define CHROME_CHROME_CLEANER_STRINGS_STRING_UTIL_H_

#include <wchar.h>

#include <set>
#include <string>

namespace chrome_cleaner {

struct WStringInsensitiveLess {
  bool operator()(const std::wstring& str1, const std::wstring& str2) const {
    return _wcsicmp(str1.c_str(), str2.c_str()) < 0;
  }
};

typedef std::set<std::wstring, WStringInsensitiveLess>
    WStringCaseInsensitiveSet;

// A function that returns true if |pattern| matches |value|, for some
// definition of "matches".
typedef bool (*WStringMatcher)(const std::wstring& value,
                               const std::wstring& pattern);

// Returns true when both strings are equal, ignoring the string case.
bool WStringEqualsCaseInsensitive(const std::wstring& str1,
                                  const std::wstring& str2);

// Returns true when |value| contains an occurrence of |substring|, ignoring
// the string case.
bool WStringContainsCaseInsensitive(const std::wstring& value,
                                    const std::wstring& substring);

// Splits |value| into a set of strings separated by any characters in
// |delimiters|, and returns true if any entry of the set is matched by
// |matcher|.
bool WStringSetMatchEntry(const std::wstring& value,
                          const std::wstring& delimiters,
                          const std::wstring& substring,
                          WStringMatcher matcher);

// Returns true if |text| matches |pattern|. |pattern| can contain can
// wild-cards like * and ?. |escape_char| is an escape character for * and ?.
// The ? wild-card matches exactly 1 character, while the * wild-card matches 0
// or more characters.
bool WStringWildcardMatchInsensitive(const std::wstring& text,
                                     const std::wstring& pattern,
                                     const wchar_t escape_char);

// Returns a copy of |input| without any invalid UTF8 characters.
std::string RemoveInvalidUTF8Chars(const std::string& input);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_STRINGS_STRING_UTIL_H_
