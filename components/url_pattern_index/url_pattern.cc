// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The matching logic distinguishes between the terms URL pattern and
// subpattern. A URL pattern usually stands for the full thing, e.g.
// "example.com^*path*par=val^", whereas subpattern denotes a maximal substring
// of a pattern not containing the wildcard '*' character. For the example above
// the subpatterns are: "example.com^", "path" and "par=val^".
//
// The separator placeholder '^' symbol is used in subpatterns to match any
// separator character, which is any ASCII symbol except letters, digits, and
// the following: '_', '-', '.', '%'. Note that the separator placeholder
// character '^' is itself a separator, as well as '\0'.

#include "components/url_pattern_index/url_pattern.h"

#include <stddef.h>

#include <algorithm>
#include <ostream>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_util.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "components/url_pattern_index/fuzzy_pattern_matching.h"
#include "components/url_pattern_index/string_splitter.h"
#include "url/gurl.h"
#include "url/third_party/mozilla/url_parse.h"

namespace url_pattern_index {

namespace {

constexpr char kWildcard = '*';

class IsWildcard {
 public:
  bool operator()(char c) const { return c == kWildcard; }
};

proto::UrlPatternType ConvertUrlPatternType(flat::UrlPatternType type) {
  switch (type) {
    case flat::UrlPatternType_SUBSTRING:
      return proto::URL_PATTERN_TYPE_SUBSTRING;
    case flat::UrlPatternType_WILDCARDED:
      return proto::URL_PATTERN_TYPE_WILDCARDED;
    case flat::UrlPatternType_REGEXP:
      return proto::URL_PATTERN_TYPE_REGEXP;
    default:
      return proto::URL_PATTERN_TYPE_UNSPECIFIED;
  }
}

proto::AnchorType ConvertAnchorType(flat::AnchorType type) {
  switch (type) {
    case flat::AnchorType_NONE:
      return proto::ANCHOR_TYPE_NONE;
    case flat::AnchorType_BOUNDARY:
      return proto::ANCHOR_TYPE_BOUNDARY;
    case flat::AnchorType_SUBDOMAIN:
      return proto::ANCHOR_TYPE_SUBDOMAIN;
    default:
      return proto::ANCHOR_TYPE_UNSPECIFIED;
  }
}

base::StringPiece ConvertString(const flatbuffers::String* string) {
  return string ? base::StringPiece(string->data(), string->size())
                : base::StringPiece();
}

bool HasAnyUpperAscii(base::StringPiece string) {
  return std::any_of(string.begin(), string.end(), base::IsAsciiUpper<char>);
}

// Returns whether |position| within the |url| belongs to its |host| component
// and corresponds to the beginning of a (sub-)domain.
inline bool IsSubdomainAnchored(base::StringPiece url,
                                url::Component host,
                                size_t position) {
  DCHECK_LE(position, url.size());
  const size_t host_begin = static_cast<size_t>(host.begin);
  const size_t host_end = static_cast<size_t>(host.end());
  DCHECK_LE(host_end, url.size());

  return position == host_begin ||
         (position > host_begin && position <= host_end &&
          url[position - 1] == '.');
}

// Returns the position of the leftmost occurrence of a |subpattern| in the
// |text| starting no earlier than |from| the specified position. If the
// |subpattern| has separator placeholders, searches for a fuzzy occurrence.
size_t FindSubpattern(base::StringPiece text,
                      base::StringPiece subpattern,
                      size_t from = 0) {
  const bool is_fuzzy =
      (subpattern.find(kSeparatorPlaceholder) != base::StringPiece::npos);
  return is_fuzzy ? FindFuzzy(text, subpattern, from)
                  : text.find(subpattern, from);
}

// Same as FindSubpattern(url, subpattern), but searches for an occurrence that
// starts at the beginning of a (sub-)domain within the url's |host| component.
size_t FindSubdomainAnchoredSubpattern(base::StringPiece url,
                                       url::Component host,
                                       base::StringPiece subpattern) {
  const bool is_fuzzy =
      (subpattern.find(kSeparatorPlaceholder) != base::StringPiece::npos);

  // Any match found after the end of the host will be discarded, so just
  // avoid searching there for the subpattern to begin with.
  //
  // Check for overflow.
  size_t max_match_end = 0;
  if (!base::CheckAdd(host.end(), subpattern.length())
           .AssignIfValid(&max_match_end)) {
    return base::StringPiece::npos;
  }
  const base::StringPiece url_match_candidate = url.substr(0, max_match_end);
  const base::StringPiece url_host = url.substr(0, host.end());

  for (size_t position = static_cast<size_t>(host.begin);
       position <= static_cast<size_t>(host.end()); ++position) {
    // Enforce as a loop precondition that we are always anchored at a
    // sub-domain before calling find. This is to reduce the number of potential
    // searches for |subpattern|.
    DCHECK(IsSubdomainAnchored(url, host, position));

    position = is_fuzzy ? FindFuzzy(url_match_candidate, subpattern, position)
                        : url_match_candidate.find(subpattern, position);
    if (position == base::StringPiece::npos ||
        IsSubdomainAnchored(url, host, position)) {
      return position;
    }

    // Enforce the loop precondition. This skips |position| to the next '.',
    // within the host, which the loop itself increments to the anchored
    // sub-domain.
    position = url_host.find('.', position);
    if (position == base::StringPiece::npos)
      break;
  }
  return base::StringPiece::npos;
}

// Helper for DoesTextMatchLastSubpattern. Treats kSeparatorPlaceholder as not
// matching the end of the text.
bool DoesTextMatchLastSubpatternInternal(proto::AnchorType anchor_left,
                                         proto::AnchorType anchor_right,
                                         base::StringPiece text,
                                         url::Component url_host,
                                         base::StringPiece subpattern) {
  // Enumerate all possible combinations of |anchor_left| and |anchor_right|.
  if (anchor_left == proto::ANCHOR_TYPE_NONE &&
      anchor_right == proto::ANCHOR_TYPE_NONE) {
    return FindSubpattern(text, subpattern) != base::StringPiece::npos;
  }

  if (anchor_left == proto::ANCHOR_TYPE_NONE &&
      anchor_right == proto::ANCHOR_TYPE_BOUNDARY) {
    return EndsWithFuzzy(text, subpattern);
  }

  if (anchor_left == proto::ANCHOR_TYPE_BOUNDARY &&
      anchor_right == proto::ANCHOR_TYPE_NONE) {
    return StartsWithFuzzy(text, subpattern);
  }

  if (anchor_left == proto::ANCHOR_TYPE_BOUNDARY &&
      anchor_right == proto::ANCHOR_TYPE_BOUNDARY) {
    return text.size() == subpattern.size() &&
           StartsWithFuzzy(text, subpattern);
  }

  if (anchor_left == proto::ANCHOR_TYPE_SUBDOMAIN &&
      anchor_right == proto::ANCHOR_TYPE_NONE) {
    return url_host.is_nonempty() &&
           FindSubdomainAnchoredSubpattern(text, url_host, subpattern) !=
               base::StringPiece::npos;
  }

  if (anchor_left == proto::ANCHOR_TYPE_SUBDOMAIN &&
      anchor_right == proto::ANCHOR_TYPE_BOUNDARY) {
    return url_host.is_nonempty() && text.size() >= subpattern.size() &&
           IsSubdomainAnchored(text, url_host,
                               text.size() - subpattern.size()) &&
           EndsWithFuzzy(text, subpattern);
  }

  NOTREACHED();
  return false;
}

// Matches the last |subpattern| against |text|. Special treatment is required
// for the last subpattern since |kSeparatorPlaceholder| can also match the end
// of the text.
bool DoesTextMatchLastSubpattern(proto::AnchorType anchor_left,
                                 proto::AnchorType anchor_right,
                                 base::StringPiece text,
                                 url::Component url_host,
                                 base::StringPiece subpattern) {
  DCHECK(!subpattern.empty());

  if (DoesTextMatchLastSubpatternInternal(anchor_left, anchor_right, text,
                                          url_host, subpattern)) {
    return true;
  }

  // If the last |subpattern| ends with kSeparatorPlaceholder, then it can also
  // match the end of text.
  if (subpattern.back() == kSeparatorPlaceholder) {
    subpattern.remove_suffix(1);
    return DoesTextMatchLastSubpatternInternal(
        anchor_left, proto::ANCHOR_TYPE_BOUNDARY, text, url_host, subpattern);
  }

  return false;
}

// Returns whether the given |url_pattern| matches the given |url_spec|.
// Compares the pattern the the url in a case-sensitive manner.
bool IsCaseSensitiveMatch(base::StringPiece url_pattern,
                          proto::AnchorType anchor_left,
                          proto::AnchorType anchor_right,
                          base::StringPiece url_spec,
                          url::Component url_host) {
  DCHECK(!url_spec.empty());

  StringSplitter<IsWildcard> subpatterns(url_pattern);
  auto subpattern_it = subpatterns.begin();
  auto subpattern_end = subpatterns.end();

  // No subpatterns.
  if (subpattern_it == subpattern_end) {
    return anchor_left == proto::ANCHOR_TYPE_NONE ||
           anchor_right == proto::ANCHOR_TYPE_NONE;
  }

  base::StringPiece subpattern = *subpattern_it;
  ++subpattern_it;

  // There is only one |subpattern|.
  if (subpattern_it == subpattern_end) {
    return DoesTextMatchLastSubpattern(anchor_left, anchor_right, url_spec,
                                       url_host, subpattern);
  }

  // Otherwise, the first |subpattern| does not have to be a suffix. But it
  // still can have a left anchor. Check and handle that.
  base::StringPiece text = url_spec;
  if (anchor_left == proto::ANCHOR_TYPE_BOUNDARY) {
    if (!StartsWithFuzzy(url_spec, subpattern))
      return false;
    text.remove_prefix(subpattern.size());
  } else if (anchor_left == proto::ANCHOR_TYPE_SUBDOMAIN) {
    if (!url_host.is_nonempty())
      return false;
    const size_t match_begin =
        FindSubdomainAnchoredSubpattern(url_spec, url_host, subpattern);
    if (match_begin == base::StringPiece::npos)
      return false;
    text.remove_prefix(match_begin + subpattern.size());
  } else {
    DCHECK_EQ(anchor_left, proto::ANCHOR_TYPE_NONE);
    // Get back to the initial |subpattern|, process it in the loop below.
    subpattern_it = subpatterns.begin();
  }

  DCHECK(subpattern_it != subpattern_end);
  subpattern = *subpattern_it;

  // Consecutively find all the remaining subpatterns in the |text|. Handle the
  // last subpattern outside the loop.
  while (++subpattern_it != subpattern_end) {
    DCHECK(!subpattern.empty());

    const size_t match_position = FindSubpattern(text, subpattern);
    if (match_position == base::StringPiece::npos)
      return false;
    text.remove_prefix(match_position + subpattern.size());

    subpattern = *subpattern_it;
  }

  return DoesTextMatchLastSubpattern(proto::ANCHOR_TYPE_NONE, anchor_right,
                                     text, url::Component(), subpattern);
}

}  // namespace

UrlPattern::UrlInfo::UrlInfo(const GURL& url)
    : spec_(url.possibly_invalid_spec()),
      host_(url.parsed_for_possibly_invalid_spec().host) {
  DCHECK(url.is_valid());
}

base::StringPiece UrlPattern::UrlInfo::GetLowerCaseSpec() const {
  if (lower_case_spec_cached_)
    return *lower_case_spec_cached_;

  if (!HasAnyUpperAscii(spec_)) {
    lower_case_spec_cached_ = spec_;
  } else {
    lower_case_spec_owner_ = base::ToLowerASCII(spec_);
    lower_case_spec_cached_ = lower_case_spec_owner_;
  }
  return *lower_case_spec_cached_;
}

UrlPattern::UrlInfo::~UrlInfo() = default;

UrlPattern::UrlPattern() = default;

UrlPattern::UrlPattern(base::StringPiece url_pattern,
                       proto::UrlPatternType type,
                       MatchCase match_case)
    : type_(type), url_pattern_(url_pattern), match_case_(match_case) {}

UrlPattern::UrlPattern(base::StringPiece url_pattern,
                       proto::AnchorType anchor_left,
                       proto::AnchorType anchor_right)
    : type_(proto::URL_PATTERN_TYPE_WILDCARDED),
      url_pattern_(url_pattern),
      anchor_left_(anchor_left),
      anchor_right_(anchor_right) {}

UrlPattern::UrlPattern(const flat::UrlRule& rule)
    : type_(ConvertUrlPatternType(rule.url_pattern_type())),
      url_pattern_(ConvertString(rule.url_pattern())),
      anchor_left_(ConvertAnchorType(rule.anchor_left())),
      anchor_right_(ConvertAnchorType(rule.anchor_right())),
      match_case_(rule.options() & flat::OptionFlag_IS_CASE_INSENSITIVE
                      ? MatchCase::kFalse
                      : MatchCase::kTrue) {}

UrlPattern::~UrlPattern() = default;

bool UrlPattern::MatchesUrl(const UrlInfo& url) const {
  DCHECK(type_ == proto::URL_PATTERN_TYPE_SUBSTRING ||
         type_ == proto::URL_PATTERN_TYPE_WILDCARDED);
  DCHECK(base::IsStringASCII(url_pattern_));
  DCHECK(base::IsStringASCII(url.spec()));
  DCHECK(base::IsStringASCII(url.GetLowerCaseSpec()));

  // Pre-process patterns to ensure left anchored and right anchored patterns
  // don't begin and end with a wildcard respectively i.e. change "|*xyz" to
  // "*xyz" and "xyz*|" to "xyz*".
  proto::AnchorType anchor_left = anchor_left_;
  proto::AnchorType anchor_right = anchor_right_;
  if (!url_pattern_.empty()) {
    if (url_pattern_.front() == kWildcard) {
      // Note: We don't handle "||*" and expect clients to disallow it.
      DCHECK_NE(proto::ANCHOR_TYPE_SUBDOMAIN, anchor_left_);
      anchor_left = proto::ANCHOR_TYPE_NONE;
    }
    if (url_pattern_.back() == kWildcard)
      anchor_right = proto::ANCHOR_TYPE_NONE;
  }

  if (match_case()) {
    return IsCaseSensitiveMatch(url_pattern_, anchor_left, anchor_right,
                                url.spec(), url.host());
  }

  // Use the lower-cased url for case-insensitive comparison. Case-insensitive
  // patterns should already be lower-cased.
  DCHECK(!HasAnyUpperAscii(url_pattern_));
  return IsCaseSensitiveMatch(url_pattern_, anchor_left, anchor_right,
                              url.GetLowerCaseSpec(), url.host());
}

std::ostream& operator<<(std::ostream& out, const UrlPattern& pattern) {
  switch (pattern.anchor_left()) {
    case proto::ANCHOR_TYPE_SUBDOMAIN:
      out << '|';
      FALLTHROUGH;
    case proto::ANCHOR_TYPE_BOUNDARY:
      out << '|';
      FALLTHROUGH;
    default:
      break;
  }
  out << pattern.url_pattern();
  if (pattern.anchor_right() == proto::ANCHOR_TYPE_BOUNDARY)
    out << '|';
  if (pattern.match_case())
    out << "$match-case";

  return out;
}

}  // namespace url_pattern_index
