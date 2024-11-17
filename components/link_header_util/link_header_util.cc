// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/link_header_util/link_header_util.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "net/http/http_util.h"

namespace link_header_util {

namespace {

// A variation of base::StringTokenizer and net::HttpUtil::ValuesIterator.
// Takes the parsing of StringTokenizer and adds support for quoted strings that
// are quoted by matching <> (and does not support escaping in those strings).
// Also has the behavior of ValuesIterator where it strips whitespace from all
// values and only outputs non-empty values.
// Only supports ',' as separator and supports "" and <> as quote chars.
class ValueTokenizer {
 public:
  ValueTokenizer(std::string::const_iterator begin,
                 std::string::const_iterator end)
      : token_begin_(begin), token_end_(begin), end_(end) {}

  std::string::const_iterator token_begin() const { return token_begin_; }
  std::string::const_iterator token_end() const { return token_end_; }

  bool GetNext() {
    while (GetNextInternal()) {
      net::HttpUtil::TrimLWS(&token_begin_, &token_end_);

      // Only return non-empty values.
      if (token_begin_ != token_end_)
        return true;
    }
    return false;
  }

 private:
  // Updates token_begin_ and token_end_ to point to the (possibly empty) next
  // token. Returns false if end-of-string was reached first.
  bool GetNextInternal() {
    // First time this is called token_end_ points to the first character in the
    // input. Every other time token_end_ points to the delimiter at the end of
    // the last returned token (which could be the end of the string).

    // End of string, return false.
    if (token_end_ == end_)
      return false;

    // Skip past the delimiter.
    if (*token_end_ == ',')
      ++token_end_;

    // Make token_begin_ point to the beginning of the next token, and search
    // for the end of the token in token_end_.
    token_begin_ = token_end_;

    // Set to true if we're currently inside a quoted string.
    bool in_quote = false;
    // Set to true if we're currently inside a quoted string, and have just
    // encountered an escape character. In this case a closing quote will be
    // ignored.
    bool in_escape = false;
    // If currently in a quoted string, this is the character that (when not
    // escaped) indicates the end of the string.
    char quote_close_char = '\0';
    // If currently in a quoted string, this is set to true if it is possible to
    // escape the closing quote using '\'.
    bool quote_allows_escape = false;

    while (token_end_ != end_) {
      char c = *token_end_;
      if (in_quote) {
        if (in_escape) {
          in_escape = false;
        } else if (quote_allows_escape && c == '\\') {
          in_escape = true;
        } else if (c == quote_close_char) {
          in_quote = false;
        }
      } else {
        if (c == ',')
          break;
        if (c == '"' || c == '<') {
          in_quote = true;
          quote_close_char = (c == '<' ? '>' : c);
          quote_allows_escape = (c != '<');
        }
      }
      ++token_end_;
    }
    return true;
  }

  std::string::const_iterator token_begin_;
  std::string::const_iterator token_end_;
  std::string::const_iterator end_;
};

// Parses the URL part of a Link header. When successful |url_begin| points
// to the beginning of the url, |url_end| points to the end of the url and
// |params_begin| points to the first character after the '>' character at the
// end of the url.
bool ExtractURL(std::string::const_iterator begin,
                std::string::const_iterator end,
                std::string::const_iterator* url_begin,
                std::string::const_iterator* url_end,
                std::string::const_iterator* params_begin) {
  // Extract the URL part (everything between '<' and first '>' character).
  if (*begin != '<')
    return false;

  ++begin;
  *url_begin = begin;
  *url_end = std::find(begin, end, '>');

  // Fail if we did not find a '>'.
  if (*url_end == end)
    return false;

  *params_begin = *url_end;
  // Skip the '>' at the end of the URL.
  ++*params_begin;

  // Trim whitespace from the URL.
  net::HttpUtil::TrimLWS(url_begin, url_end);
  return true;
}

}  // namespace

std::vector<StringIteratorPair> SplitLinkHeader(const std::string& header) {
  std::vector<StringIteratorPair> values;
  ValueTokenizer tokenizer(header.begin(), header.end());
  while (tokenizer.GetNext()) {
    values.push_back(
        StringIteratorPair(tokenizer.token_begin(), tokenizer.token_end()));
  }
  return values;
}

// Parses one link in a link header into its url and parameters.
// A link is of the form "<some-url>; param1=value1; param2=value2".
// Returns false if parsing the link failed, returns true on success. This
// method is more lenient than the RFC. It doesn't fail on things like invalid
// characters in the URL, and also doesn't verify that certain parameters should
// or shouldn't be quoted strings.
// If a parameter occurs more than once in the link, only the first value is
// returned in params as this is the required behavior for all attributes chrome
// currently cares about in link headers.
bool ParseLinkHeaderValue(
    std::string::const_iterator begin,
    std::string::const_iterator end,
    std::string* url,
    std::unordered_map<std::string, std::optional<std::string>>* params) {
  // Can't parse an empty string.
  if (begin == end)
    return false;

  // Extract the URL part (everything between '<' and first '>' character).
  std::string::const_iterator url_begin;
  std::string::const_iterator url_end;
  if (!ExtractURL(begin, end, &url_begin, &url_end, &begin))
    return false;
  *url = std::string(url_begin, url_end);

  // Trim any remaining whitespace, and make sure there is a ';' separating
  // parameters from the URL.
  std::string_view value = net::HttpUtil::TrimLWS(std::string_view(begin, end));
  if (!value.empty() && value.front() != ';') {
    return false;
  }

  // Parse all the parameters.
  net::HttpUtil::NameValuePairsIterator params_iterator(
      value, /*delimiter=*/';',
      net::HttpUtil::NameValuePairsIterator::Values::NOT_REQUIRED,
      net::HttpUtil::NameValuePairsIterator::Quotes::STRICT_QUOTES);
  while (params_iterator.GetNext()) {
    if (!net::HttpUtil::IsParmName(params_iterator.name())) {
      return false;
    }
    std::string name = base::ToLowerASCII(params_iterator.name());
    if (!params_iterator.value_is_quoted() && params_iterator.value().empty()) {
      params->insert(std::make_pair(name, std::nullopt));
    } else {
      params->insert(std::make_pair(name, params_iterator.value()));
    }
  }
  return params_iterator.valid();
}

}  // namespace link_header_util
