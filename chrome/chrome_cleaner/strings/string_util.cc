// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/strings/string_util.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/i18n/streaming_utf8_validator.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace chrome_cleaner {

namespace {

// A map from (text offset, pattern offest) to bool.
typedef std::map<std::pair<size_t, size_t>, bool> WildcardMatchCache;

bool WStringWildcardMatchRecursive(const std::wstring& text,
                                   size_t text_offset,
                                   const std::wstring& pattern,
                                   size_t pattern_offset,
                                   const wchar_t escape_char,
                                   WildcardMatchCache* cache);

bool WStringWildcardMatchRecursiveCached(const std::wstring& text,
                                         size_t text_offset,
                                         const std::wstring& pattern,
                                         size_t pattern_offset,
                                         const wchar_t escape_char,
                                         WildcardMatchCache* cache) {
  DCHECK(cache);

  // Look for a pre-computed value.
  WildcardMatchCache::key_type key =
      std::make_pair(text_offset, pattern_offset);
  WildcardMatchCache::iterator entry = cache->find(key);
  if (entry != cache->end())
    return entry->second;

  // Compute the recursive match and cache it.
  bool result = WStringWildcardMatchRecursive(
      text, text_offset, pattern, pattern_offset, escape_char, cache);
  (*cache)[key] = result;
  return result;
}

bool WStringWildcardMatchRecursive(const std::wstring& text,
                                   size_t text_offset,
                                   const std::wstring& pattern,
                                   size_t pattern_offset,
                                   const wchar_t escape_char,
                                   WildcardMatchCache* cache) {
  while (true) {
    if (text_offset >= text.length()) {
      // The text is empty, the matching decision is based on the remaining
      // pattern.
      if (pattern_offset >= pattern.length()) {
        // The pattern is empty. The match is successful.
        return true;
      } else if (pattern[pattern_offset] == L'*') {
        // Multi-characters wild-card can match empty string, skip it.
        pattern_offset++;
        continue;
      } else {
        // There is no way to match the remaining pattern.
        return false;
      }
    } else if (pattern_offset >= pattern.length()) {
      // The text isn't empty but the pattern is, failed to match.
      return false;
    }

    // Retrieve first characters.
    wchar_t first_char = text[text_offset];
    wchar_t first_pattern = pattern[pattern_offset];

    if (first_pattern == L'?') {
      // Eat this character and move to the next one.
      ++text_offset;
      ++pattern_offset;
    } else if (first_pattern == L'*') {
      // The multi-character wild-card can match any number of characters. For
      // each remaining offset, try to recursively match at that position the
      // rest of the pattern. The maximal offset is one after the last
      // character.
      for (size_t match_offset = text_offset; match_offset <= text.size();
           ++match_offset) {
        if (WStringWildcardMatchRecursiveCached(text, match_offset, pattern,
                                                pattern_offset + 1, escape_char,
                                                cache)) {
          return true;
        }
      }
      return false;
    } else {
      // Skip escaping character
      if (first_pattern == escape_char) {
        if (pattern_offset + 1 >= pattern.size()) {
          // The pattern has an escape character without a following character.
          return false;
        }
        ++pattern_offset;
        first_pattern = pattern[pattern_offset];
        DCHECK(first_pattern == escape_char || first_pattern == L'?' ||
               first_pattern == L'*');
      }

      // Perform a case insensitive comparison between both characters.
      if (base::ToUpperASCII(first_char) != base::ToUpperASCII(first_pattern))
        return false;

      // Eat this character and move to the next one.
      ++text_offset;
      ++pattern_offset;
    }
  }
}

}  // namespace

bool WStringEqualsCaseInsensitive(const std::wstring& str1,
                                  const std::wstring& str2) {
  return _wcsicmp(str1.c_str(), str2.c_str()) == 0;
}

bool WStringContainsCaseInsensitive(const std::wstring& value,
                                    const std::wstring& substring) {
  return base::ranges::search(
             value, substring,
             base::CaseInsensitiveCompareASCII<std::wstring::value_type>()) !=
         value.end();
}

bool WStringSetMatchEntry(const std::wstring& value,
                          const std::wstring& delimiters,
                          const std::wstring& substring,
                          WStringMatcher matcher) {
  // Split the string in tokens.
  std::vector<std::wstring> tokens = base::SplitString(
      value, delimiters, base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Search for a matching token.
  for (std::vector<std::wstring>::const_iterator it = tokens.begin();
       it != tokens.end(); ++it) {
    if (matcher(*it, substring))
      return true;
  }
  return false;
}

bool WStringWildcardMatchInsensitive(const std::wstring& text,
                                     const std::wstring& pattern,
                                     const wchar_t escape_char) {
  // TODO(crbug.com/837637): Check the performance of Chromium's MatchPattern
  // and replace this with it if possible.
  WildcardMatchCache cache;
  return WStringWildcardMatchRecursive(text, 0, pattern, 0, escape_char,
                                       &cache);
}

std::string RemoveInvalidUTF8Chars(const std::string& input) {
  std::string utf8_output;
  size_t mid_point = 0;
  base::StreamingUtf8Validator utf8_validator;
  for (size_t str_index = 0; str_index < input.size(); ++str_index) {
    switch (utf8_validator.AddBytes(&input[str_index], 1)) {
      case base::StreamingUtf8Validator::VALID_ENDPOINT:
        for (size_t offset = mid_point; offset > 0; --offset)
          utf8_output += input[str_index - offset];
        utf8_output += input[str_index];
        mid_point = 0;
        break;
      case base::StreamingUtf8Validator::VALID_MIDPOINT:
        mid_point++;
        break;
      case base::StreamingUtf8Validator::INVALID:
        utf8_validator.Reset();
        mid_point = 0;
        break;
    }
  }
  return utf8_output;
}

}  // namespace chrome_cleaner
