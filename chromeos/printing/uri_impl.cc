// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/printing/uri_impl.h"

#include <algorithm>
#include <array>
#include <set>

#include "base/check_op.h"
#include "base/i18n/streaming_utf8_validator.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "chromeos/printing/uri.h"

namespace chromeos {

namespace {

constexpr int kPortInvalid = -2;
constexpr int kPortUnspecified = -1;
constexpr int kPortMaxNumber = 65535;

// Parses a single character from *|current| and interprets it as a hex
// digit ('0'-'9' or 'A'-'F' or 'a'-'f'). If the character is incorrect or
// *|current| is not less than |end|, the function returns false.
// Otherwise, the value in *|out| is shifted left by 4 bits and the parsed
// value is saved on its rightmost 4 bits. The iterator *|current| is
// increased by one, and the function returns true.
// |current| and |out| must be not nullptr.
bool ParseHexDigit(const Iter& end, Iter* current, unsigned char* out) {
  Iter& it = *current;
  if (it >= end)
    return false;
  *out <<= 4;
  if (base::IsAsciiDigit(*it)) {
    *out += (*it - '0');
  } else if (*it >= 'A' && *it <= 'F') {
    *out += (*it - 'A' + 10);
  } else if (*it >= 'a' && *it <= 'f') {
    *out += (*it - 'a' + 10);
  } else {
    return false;
  }
  ++it;
  return true;
}

// The function parses from *|current|-|end| the first character and saves it
// to |out|. If |encoded| equals true, the % sign is treated as the beginning
// of %-escaped character - in this case the whole escaped character is read
// and decoded. The function fails and returns false when unexpected end of
// string is reached or invalid %-escaped character is spotted. The iterator
// *|current| is shifted accordingly.
// |current| and |out| must be not nullptr and *|current| must be less than
// |end|.
template <bool encoded>
bool ParseCharacter(const Iter& end, Iter* current, char* out) {
  Iter& it = *current;
  DCHECK(it < end);
  *out = *it;
  ++it;
  if (encoded && *out == '%') {
    unsigned char c = 0;
    if (!ParseHexDigit(end, &it, &c))
      return false;
    if (!ParseHexDigit(end, &it, &c))
      return false;
    *out = static_cast<char>(c);
  }
  return true;
}

// Returns iterator to the first occurrence of any character from |chars|
// in |begin|-|end|. Returns |end| if none of the characters were found.
Iter FindFirstOf(Iter begin, Iter end, const std::string& chars) {
  return std::find_first_of(begin, end, chars.begin(), chars.end());
}

}  // namespace

template <bool encoded, bool case_insensitive>
bool Uri::Pim::ParseString(const Iter& begin,
                           const Iter& end,
                           std::string* out,
                           bool plus_to_space) {
  parser_error_.parsed_chars = 0;
  out->reserve(end - begin);
  for (Iter it = begin; it < end;) {
    char c;
    // Read and decode a single character or a %-escaped character.
    if (plus_to_space && *it == '+') {
      c = ' ';
      ++it;
    } else if (!ParseCharacter<encoded>(end, &it, &c)) {
      parser_error_.status = ParserStatus::kInvalidPercentEncoding;
      return false;
    }
    // Analyze the character.
    if (base::IsAsciiPrintable(c)) {  // c >= 0x20(' ') && c <= 0x7E('~')
      // Copy the character with normalization.
      out->push_back(case_insensitive ? base::ToLowerASCII(c) : c);
      parser_error_.parsed_chars = it - begin;
    } else {
      // Try to parse UTF-8 character.
      base::StreamingUtf8Validator utf_parser;
      base::StreamingUtf8Validator::State state =
          utf_parser.AddBytes(base::byte_span_from_ref(c));
      if (state != base::StreamingUtf8Validator::State::VALID_MIDPOINT) {
        parser_error_.status = ParserStatus::kDisallowedASCIICharacter;
        return false;
      }
      std::string utf8_character(1, c);
      parser_error_.parsed_chars = it - begin;
      do {
        if (it == end) {
          parser_error_.status = ParserStatus::kInvalidUTF8Character;
          return false;
        }
        if (!ParseCharacter<encoded>(end, &it, &c)) {
          parser_error_.status = ParserStatus::kInvalidPercentEncoding;
          return false;
        }
        state = utf_parser.AddBytes(base::byte_span_from_ref(c));
        if (state == base::StreamingUtf8Validator::State::INVALID) {
          parser_error_.status = ParserStatus::kInvalidUTF8Character;
          return false;
        }
        utf8_character.push_back(c);
        parser_error_.parsed_chars = it - begin;
      } while (state != base::StreamingUtf8Validator::State::VALID_ENDPOINT);
      // Saves the UTF-8 character to the output.
      out->append(std::move(utf8_character));
    }
  }
  ++(parser_error_.parsed_strings);
  return true;
}

template <bool encoded>
bool Uri::Pim::SaveUserinfo(const std::string& val) {
  parser_error_.status = ParserStatus::kNoErrors;
  parser_error_.parsed_strings = 0;
  std::string out;
  if (!ParseString<encoded>(val.begin(), val.end(), &out))
    return false;
  userinfo_ = std::move(out);
  return true;
}

template <bool encoded>
bool Uri::Pim::SaveHost(const std::string& val) {
  parser_error_.status = ParserStatus::kNoErrors;
  parser_error_.parsed_strings = 0;
  std::string out;
  if (!ParseString<encoded, true>(val.begin(), val.end(), &out))
    return false;
  host_ = std::move(out);
  return true;
}

bool Uri::Pim::SavePort(int value) {
  parser_error_.status = ParserStatus::kNoErrors;
  parser_error_.parsed_strings = 0;
  parser_error_.parsed_chars = 0;
  if (value < -1 || value > 65535) {
    parser_error_.status = ParserStatus::kInvalidPortNumber;
    return false;
  }
  if (value == kPortUnspecified)
    value = Uri::GetDefaultPort(scheme_);
  port_ = value;
  return true;
}

template <bool encoded>
bool Uri::Pim::SavePath(const std::vector<std::string>& val) {
  parser_error_.status = ParserStatus::kNoErrors;
  parser_error_.parsed_strings = 0;
  parser_error_.parsed_chars = 0;
  std::vector<std::string> out;
  out.reserve(val.size());
  for (size_t i = 0; i < val.size(); ++i) {
    std::string segment;
    auto it1 = val[i].begin();
    auto it2 = val[i].end();
    if (!ParseString<encoded>(it1, it2, &segment))
      return false;
    if (segment == ".") {
      // do nothing
    } else if (segment == ".." && !out.empty() && out.back() != "..") {
      out.pop_back();
    } else if (segment.empty()) {
      --parser_error_.parsed_strings;  // it was already counted
      parser_error_.parsed_chars = 0;
      parser_error_.status = ParserStatus::kEmptySegmentInPath;
      return false;
    } else {
      out.push_back(std::move(segment));
    }
  }
  path_ = std::move(out);
  return true;
}

template <bool encoded>
bool Uri::Pim::SaveQuery(
    const std::vector<std::pair<std::string, std::string>>& val) {
  parser_error_.status = ParserStatus::kNoErrors;
  parser_error_.parsed_strings = 0;
  parser_error_.parsed_chars = 0;
  std::vector<std::pair<std::string, std::string>> out(val.size());
  for (size_t i = 0; i < out.size(); ++i) {
    // Process parameter name.
    auto it1 = val[i].first.begin();
    auto it2 = val[i].first.end();
    if (!ParseString<encoded>(it1, it2, &out[i].first, encoded))
      return false;
    if (out[i].first.empty()) {
      --parser_error_.parsed_strings;  // it was already counted
      parser_error_.parsed_chars = 0;
      parser_error_.status = ParserStatus::kEmptyParameterNameInQuery;
      return false;
    }
    // Process parameter value.
    it1 = val[i].second.begin();
    it2 = val[i].second.end();
    if (!ParseString<encoded>(it1, it2, &out[i].second, encoded))
      return false;
  }
  query_ = std::move(out);
  return true;
}

template <bool encoded>
bool Uri::Pim::SaveFragment(const std::string& val) {
  parser_error_.status = ParserStatus::kNoErrors;
  parser_error_.parsed_strings = 0;
  std::string out;
  if (!ParseString<encoded>(val.begin(), val.end(), &out))
    return false;
  fragment_ = std::move(out);
  return true;
}

bool Uri::Pim::ParseScheme(const Iter& begin, const Iter& end) {
  parser_error_.status = ParserStatus::kNoErrors;
  parser_error_.parsed_strings = 0;
  parser_error_.parsed_chars = 0;
  // Special case for an empty string on the input.
  if (begin == end) {
    scheme_.clear();
    return true;
  }
  // Temporary output string.
  std::string out;
  out.reserve(end - begin);
  // Checks the first character - must be an ASCII letter.
  auto it = begin;
  if (base::IsAsciiAlpha(*it)) {
    out.push_back(base::ToLowerASCII(*it));
  } else {
    parser_error_.status = ParserStatus::kInvalidScheme;
    return false;
  }
  // Checks the rest of characters.
  for (++it; it < end; ++it) {
    if (base::IsAsciiAlpha(*it) || base::IsAsciiDigit(*it) || *it == '+' ||
        *it == '-' || *it == '.') {
      out.push_back(base::ToLowerASCII(*it));
    } else {
      parser_error_.status = ParserStatus::kInvalidScheme;
      parser_error_.parsed_chars = it - begin;
      return false;
    }
  }
  // Success - save the Scheme.
  scheme_ = std::move(out);
  // If the current Port is unspecified and the new Scheme has default port
  // number, set the default port number.
  if (port_ == kPortUnspecified)
    port_ = Uri::GetDefaultPort(scheme_);
  return true;
}

bool Uri::Pim::ParseAuthority(const Iter& begin, const Iter& end) {
  // Parse and save Userinfo.
  Iter it = std::find(begin, end, '@');
  if (it != end) {
    if (!SaveUserinfo<true>(std::string(begin, it))) {
      parser_error_.parsed_chars += it - begin;
      return false;
    }
    ++it;  // to omit '@' character
  } else {
    it = begin;
  }
  // Parse and save Host.
  Iter it2 = std::find(it, end, ':');
  if (!SaveHost<true>(std::string(it, it2))) {
    parser_error_.parsed_chars += it - begin;
    return false;
  }
  // Parse and save Port.
  if (it2 != end) {
    ++it2;  // omit the ':' character
    if (!ParsePort(it2, end)) {
      parser_error_.parsed_chars += it2 - begin;
      return false;
    }
  }
  return true;
}

bool Uri::Pim::ParsePort(const Iter& begin, const Iter& end) {
  if (begin == end)
    return SavePort(kPortUnspecified);
  int number = 0;
  for (Iter it = begin; it < end; ++it) {
    if (!base::IsAsciiDigit(*it))
      return SavePort(kPortInvalid);
    number *= 10;
    number += *it - '0';
    if (number > kPortMaxNumber)
      return SavePort(kPortInvalid);
  }
  return SavePort(number);
}

bool Uri::Pim::ParsePath(const Iter& begin, const Iter& end) {
  // Path must be empty or start with '/'.
  if (begin < end && *begin != '/') {
    parser_error_.status = ParserStatus::kRelativePathsNotAllowed;
    parser_error_.parsed_chars = 0;
    parser_error_.parsed_strings = 0;
    return false;
  }
  // This holds Path's segments.
  std::vector<std::string> path;
  // This stores offset from begin of every segment.
  std::vector<size_t> strings_positions;
  // Parsing...
  for (Iter it1 = begin; it1 < end;) {
    if (++it1 == end)  // omit '/' character
      break;
    Iter it2 = std::find(it1, end, '/');
    path.push_back(std::string(it1, it2));
    strings_positions.push_back(it1 - begin);
    it1 = it2;
  }
  // Try to set the new Path and return true if succeed.
  if (SavePath<true>(path))
    return true;
  // An error occurred, adjust parser error fields set by SetPath(...).
  parser_error_.parsed_chars += strings_positions[parser_error_.parsed_strings];
  parser_error_.parsed_strings = 0;
  return false;
}

bool Uri::Pim::ParseQuery(const Iter& begin, const Iter& end) {
  // This holds pairs name=value.
  std::vector<std::pair<std::string, std::string>> query;
  // This stores offset from begin of every name and value.
  std::vector<size_t> strings_positions;
  // Parsing...
  for (Iter it = begin; it < end;) {
    Iter it_am = std::find(it, end, '&');
    Iter it_eq = std::find(it, it_am, '=');
    // Extract name.
    std::string name(it, it_eq);
    // Extract value.
    if (it_eq < it_am)  // to omit '=' character
      ++it_eq;
    std::string value(it_eq, it_am);
    // Save the pair (name,value).
    query.push_back(std::make_pair(std::move(name), std::move(value)));
    // Store the offset of the name.
    strings_positions.push_back(it - begin);
    // Store the offset of the value.
    strings_positions.push_back(it_eq - begin);
    // Move |it| to the beginning of the next pair.
    if (it_am < end)
      ++it_am;  // to omit '&' character
    it = it_am;
  }
  // Try to set the new Query and return true if succeed.
  if (SaveQuery<true>(query))
    return true;
  // An error occurred, adjust the |parser_error| set by SetQuery(...).
  parser_error_.parsed_chars += strings_positions[parser_error_.parsed_strings];
  parser_error_.parsed_strings = 0;
  return false;
}

bool Uri::Pim::ParseFragment(const Iter& begin, const Iter& end) {
  parser_error_.parsed_strings = 0;
  std::string out;
  if (!ParseString<true>(begin, end, &out))
    return false;
  fragment_ = std::move(out);
  return true;
}

bool Uri::Pim::ParseUri(const Iter& begin, const Iter end) {
  parser_error_.status = ParserStatus::kNoErrors;
  parser_error_.parsed_strings = 0;
  parser_error_.parsed_chars = 0;
  Iter it1 = begin;
  // The Scheme component starts from character different than slash ("/"),
  // question mark ("?"), and number sign ("#"). Non-empty Scheme must be
  // followed by the colon (":") character.
  if (it1 < end && *it1 != '/' && *it1 != '?' && *it1 != '#') {
    auto it2 = std::find(it1, end, ':');
    if (it2 == end) {
      parser_error_.status = ParserStatus::kInvalidScheme;
      return false;
    }
    if (!ParseScheme(it1, it2))
      return false;
    it1 = ++it2;
  }
  // The authority component is preceded by a double slash ("//") and is
  // terminated by the next slash ("/"), question mark ("?"), or number
  // sign ("#") character, or by the end of the URI.
  if (it1 < end && *it1 == '/') {
    ++it1;
    if (it1 < end && *it1 == '/') {
      ++it1;
      auto it_auth_end = FindFirstOf(it1, end, "/?#");
      if (!ParseAuthority(it1, it_auth_end)) {
        parser_error_.parsed_chars += it1 - begin;
        return false;
      }
      it1 = it_auth_end;
    } else {
      --it1;
    }
  }
  // The Path is terminated by the first question mark ("?") or number
  // sign ("#") character, or by the end of the URI.
  if (it1 < end) {
    auto it2 = FindFirstOf(it1, end, "?#");
    if (!ParsePath(it1, it2)) {
      parser_error_.parsed_chars += it1 - begin;
      return false;
    }
    it1 = it2;
  }
  // The Query component is indicated by the first question mark ("?")
  // character and terminated by a number sign ("#") character or by the end
  // of the URI.
  if (it1 < end && *it1 == '?') {
    ++it1;
    auto it2 = std::find(it1, end, '#');
    if (!ParseQuery(it1, it2)) {
      parser_error_.parsed_chars += it1 - begin;
      return false;
    }
    it1 = it2;
  }
  // A Fragment component is indicated by the presence of a number
  // sign ("#") character and terminated by the end of the URI.
  if (it1 < end) {
    DCHECK_EQ(*it1, '#');
    ++it1;  // to omit '#' character
    if (!ParseFragment(it1, end)) {
      parser_error_.parsed_chars += it1 - begin;
      return false;
    }
  }
  // Success!
  return true;
}

template bool Uri::Pim::ParseString<false, false>(const Iter& begin,
                                                  const Iter& end,
                                                  std::string* out,
                                                  bool plus_to_space);
template bool Uri::Pim::ParseString<false, true>(const Iter& begin,
                                                 const Iter& end,
                                                 std::string* out,
                                                 bool plus_to_space);
template bool Uri::Pim::ParseString<true, false>(const Iter& begin,
                                                 const Iter& end,
                                                 std::string* out,
                                                 bool plus_to_space);
template bool Uri::Pim::ParseString<true, true>(const Iter& begin,
                                                const Iter& end,
                                                std::string* out,
                                                bool plus_to_space);

template bool Uri::Pim::SaveUserinfo<false>(const std::string& val);
template bool Uri::Pim::SaveUserinfo<true>(const std::string& val);

template bool Uri::Pim::SaveHost<false>(const std::string& val);
template bool Uri::Pim::SaveHost<true>(const std::string& val);

template bool Uri::Pim::SavePath<false>(const std::vector<std::string>& val);
template bool Uri::Pim::SavePath<true>(const std::vector<std::string>& val);

template bool Uri::Pim::SaveQuery<false>(
    const std::vector<std::pair<std::string, std::string>>& val);
template bool Uri::Pim::SaveQuery<true>(
    const std::vector<std::pair<std::string, std::string>>& val);

template bool Uri::Pim::SaveFragment<false>(const std::string& val);
template bool Uri::Pim::SaveFragment<true>(const std::string& val);

}  // namespace chromeos
