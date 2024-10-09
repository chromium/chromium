// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_matcher/url_matcher.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "url/gurl.h"
#include "url/url_canon.h"

using base::MatcherStringPattern;
using base::SubstringSetMatcher;

namespace url_matcher {

// This set of classes implement a mapping of URL Component Patterns, such as
// host_prefix, host_suffix, host_equals, ..., etc., to MatcherStringPatterns
// for use in substring comparisons.
//
// The idea of this mapping is to reduce the problem of comparing many
// URL Component Patterns against one URL to the problem of searching many
// substrings in one string:
//
// ----------------------                    ------------------------
// | URL Query operator | ----translate----> | MatcherStringPattern |
// ----------------------                    ------------------------
//                                                   ^
//                                                   |
//                                                compare
//                                                   |
//                                                   v
// ----------------------                    -----------------
// | URL to compare     |                    |               |
// | to all URL Query   | ----translate----> | String        |
// | operators          |                    |               |
// ----------------------                    -----------------
//
// The reason for this problem reduction is that there are efficient algorithms
// for searching many substrings in one string (see Aho-Corasick algorithm).
//
// Additionally, some of the same pieces are reused to implement regular
// expression comparisons. The FilteredRE2 implementation for matching many
// regular expressions against one string uses prefiltering, in which a set
// of substrings (derived from the regexes) are first searched for, to reduce
// the number of regular expressions to test; the prefiltering step also
// uses Aho-Corasick.
//
// Case 1: {host,path,query}_{prefix,suffix,equals} searches.
// ==========================================================
//
// For searches in this class, we normalize URLs as follows:
//
// Step 1:
// Remove scheme, port and segment from URL:
// -> http://www.example.com:8080/index.html?search=foo#first_match becomes
//    www.example.com/index.html?search=foo
//
// We remove the scheme and port number because they can be checked later
// in a secondary filter step. We remove the segment (the #... part) because
// this is not guaranteed to be ASCII-7 encoded.
//
// Step 2:
// Translate URL to String and add the following position markers:
// - BU = Beginning of URL
// - ED = End of Domain
// - EP = End of Path
// - EU = End of URL
// Furthermore, the hostname is canonicalized to start with a ".".
//
// Position markers are represented as characters >127, which are therefore
// guaranteed not to be part of the ASCII-7 encoded URL character set.
//
// -> www.example.com/index.html?search=foo becomes
// BU .www.example.com ED /index.html EP ?search=foo EU
//
// -> www.example.com/index.html becomes
// BU .www.example.com ED /index.html EP EU
//
// Step 3:
// Translate URL Component Patterns as follows:
//
// host_prefix(prefix) = BU add_missing_dot_prefix(prefix)
// -> host_prefix("www.example") = BU .www.example
//
// host_suffix(suffix) = suffix ED
// -> host_suffix("example.com") = example.com ED
// -> host_suffix(".example.com") = .example.com ED
//
// host_equals(domain) = BU add_missing_dot_prefix(domain) ED
// -> host_equals("www.example.com") = BU .www.example.com ED
//
// Similarly for path query parameters ({path, query}_{prefix, suffix, equals}).
//
// With this, we can search the MatcherStringPatterns in the normalized URL.
//
//
// Case 2: url_{prefix,suffix,equals,contains} searches.
// =====================================================
//
// Step 1: as above, except that
// - the scheme is not removed
// - the port is not removed if it is specified and does not match the default
//   port for the given scheme.
//
// Step 2:
// Translate URL to String and add the following position markers:
// - BU = Beginning of URL
// - EU = End of URL
//
// -> http://www.example.com:8080/index.html?search=foo#first_match becomes
// BU http://www.example.com:8080/index.html?search=foo EU
// -> http://www.example.com:80/index.html?search=foo#first_match becomes
// BU http://www.example.com/index.html?search=foo EU
//
// url_prefix(prefix) = BU prefix
// -> url_prefix("http://www.example") = BU http://www.example
//
// url_contains(substring) = substring
// -> url_contains("index") = index
//
//
// Case 3: {host,path,query}_contains searches.
// ============================================
//
// These kinds of searches are not supported directly but can be derived
// by a combination of a url_contains() query followed by an explicit test:
//
// host_contains(str) = url_contains(str) followed by test whether str occurs
//   in host component of original URL.
// -> host_contains("example.co") = example.co
//    followed by gurl.host().find("example.co");
//
// [similarly for path_contains and query_contains].
//
//
// Regular expression matching (url_matches searches)
// ==================================================
//
// This class also supports matching regular expressions (RE2 syntax)
// against full URLs, which are transformed as in case 2.

namespace {

bool IsRegexCriterion(URLMatcherCondition::Criterion criterion) {
  return criterion == URLMatcherCondition::URL_MATCHES;
}

bool IsOriginAndPathRegexCriterion(URLMatcherCondition::Criterion criterion) {
  return criterion == URLMatcherCondition::ORIGIN_AND_PATH_MATCHES;
}

bool IsMatcherEmpty(const std::unique_ptr<SubstringSetMatcher>& matcher) {
  return !matcher || matcher->IsEmpty();
}

}  // namespace

//
// URLMatcherCondition
//

URLMatcherCondition::URLMatcherCondition()
    : criterion_(HOST_PREFIX), string_pattern_(nullptr) {}

URLMatcherCondition::~URLMatcherCondition() = default;

URLMatcherCondition::URLMatcherCondition(
    Criterion criterion,
    const MatcherStringPattern* string_pattern)
    : criterion_(criterion), string_pattern_(string_pattern) {}

bool URLMatcherCondition::operator<(const URLMatcherCondition& rhs) const {
  if (criterion_ < rhs.criterion_)
    return true;
  if (criterion_ > rhs.criterion_)
    return false;
  if (string_pattern_ != nullptr && rhs.string_pattern_ != nullptr)
    return *string_pattern_ < *rhs.string_pattern_;
  if (string_pattern_ == nullptr && rhs.string_pattern_ != nullptr)
    return true;
  // Either string_pattern_ != NULL && rhs.string_pattern_ == NULL,
  // or both are NULL.
  return false;
}

bool URLMatcherCondition::IsFullURLCondition() const {
  // For these criteria the SubstringMatcher needs to be executed on the
  // GURL that is canonicalized with
  // URLMatcherConditionFactory::CanonicalizeURLForFullSearches.
  switch (criterion_) {
    case HOST_CONTAINS:
    case PATH_CONTAINS:
    case QUERY_CONTAINS:
    case URL_PREFIX:
    case URL_SUFFIX:
    case URL_CONTAINS:
    case URL_EQUALS:
      return true;
    default:
      break;
  }
  return false;
}

bool URLMatcherCondition::IsRegexCondition() const {
  return IsRegexCriterion(criterion_);
}

bool URLMatcherCondition::IsOriginAndPathRegexCondition() const {
  return IsOriginAndPathRegexCriterion(criterion_);
}

bool URLMatcherCondition::IsMatch(
    const std::set<MatcherStringPattern::ID>& matching_patterns,
    const GURL& url) const {
  DCHECK(string_pattern_);
  if (!base::Contains(matching_patterns, string_pattern_->id()))
    return false;
  // The criteria HOST_CONTAINS, PATH_CONTAINS, QUERY_CONTAINS are based on
  // a substring match on the raw URL. In case of a match, we need to verify
  // that the match was found in the correct component of the URL.
  switch (criterion_) {
    case HOST_CONTAINS:
      return url.host().find(string_pattern_->pattern()) != std::string::npos;
    case PATH_CONTAINS:
      return url.path().find(string_pattern_->pattern()) != std::string::npos;
    case QUERY_CONTAINS:
      return url.query().find(string_pattern_->pattern()) != std::string::npos;
    default:
      break;
  }
  return true;
}

//
// URLMatcherConditionFactory
//

namespace {
// These are symbols that are not contained in 7-bit ASCII used in GURLs.
const char kBeginningOfURL[] = {static_cast<char>(-1), 0};
const char kEndOfDomain[] = {static_cast<char>(-2), 0};
const char kEndOfPath[] = {static_cast<char>(-3), 0};
const char kQueryComponentDelimiter[] = {static_cast<char>(-4), 0};
const char kEndOfURL[] = {static_cast<char>(-5), 0};

// The delimiter for query parameters
const char kQuerySeparator = '&';
}  // namespace

URLMatcherConditionFactory::URLMatcherConditionFactory() = default;

URLMatcherConditionFactory::~URLMatcherConditionFactory() = default;

std::string URLMatcherConditionFactory::CanonicalizeURLForComponentSearches(
    const GURL& url) const {
  return kBeginningOfURL + CanonicalizeHostname(url.host()) + kEndOfDomain +
         url.path() + kEndOfPath +
         (url.has_query() ? CanonicalizeQuery(url.query(), true, true)
                          : std::string()) +
         kEndOfURL;
}

URLMatcherCondition URLMatcherConditionFactory::CreateHostPrefixCondition(
    const std::string& prefix) {
  return CreateCondition(URLMatcherCondition::HOST_PREFIX,
                         kBeginningOfURL + CanonicalizeHostPrefix(prefix));
}

URLMatcherCondition URLMatcherConditionFactory::CreateHostSuffixCondition(
    const std::string& suffix) {
  return CreateCondition(URLMatcherCondition::HOST_SUFFIX,
                         CanonicalizeHostSuffix(suffix) + kEndOfDomain);
}

URLMatcherCondition URLMatcherConditionFactory::CreateHostContainsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::HOST_CONTAINS, str);
}

URLMatcherCondition URLMatcherConditionFactory::CreateHostEqualsCondition(
    const std::string& str) {
  return CreateCondition(
      URLMatcherCondition::HOST_EQUALS,
      kBeginningOfURL + CanonicalizeHostname(str) + kEndOfDomain);
}

URLMatcherCondition URLMatcherConditionFactory::CreatePathPrefixCondition(
    const std::string& prefix) {
  return CreateCondition(URLMatcherCondition::PATH_PREFIX,
                         kEndOfDomain + prefix);
}

URLMatcherCondition URLMatcherConditionFactory::CreatePathSuffixCondition(
    const std::string& suffix) {
  return CreateCondition(URLMatcherCondition::PATH_SUFFIX, suffix + kEndOfPath);
}

URLMatcherCondition URLMatcherConditionFactory::CreatePathContainsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::PATH_CONTAINS, str);
}

URLMatcherCondition URLMatcherConditionFactory::CreatePathEqualsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::PATH_EQUALS,
                         kEndOfDomain + str + kEndOfPath);
}

URLMatcherCondition URLMatcherConditionFactory::CreateQueryPrefixCondition(
    const std::string& prefix) {
  std::string pattern;
  if (!prefix.empty() && prefix[0] == '?')
    pattern = kEndOfPath + CanonicalizeQuery(prefix.substr(1), true, false);
  else
    pattern = kEndOfPath + CanonicalizeQuery(prefix, true, false);

  return CreateCondition(URLMatcherCondition::QUERY_PREFIX, pattern);
}

URLMatcherCondition URLMatcherConditionFactory::CreateQuerySuffixCondition(
    const std::string& suffix) {
  if (!suffix.empty() && suffix[0] == '?') {
    return CreateQueryEqualsCondition(suffix);
  } else {
    return CreateCondition(URLMatcherCondition::QUERY_SUFFIX,
                           CanonicalizeQuery(suffix, false, true) + kEndOfURL);
  }
}

URLMatcherCondition URLMatcherConditionFactory::CreateQueryContainsCondition(
    const std::string& str) {
  if (!str.empty() && str[0] == '?')
    return CreateQueryPrefixCondition(str);
  else
    return CreateCondition(URLMatcherCondition::QUERY_CONTAINS, str);
}

URLMatcherCondition URLMatcherConditionFactory::CreateQueryEqualsCondition(
    const std::string& str) {
  std::string pattern;
  if (!str.empty() && str[0] == '?')
    pattern =
        kEndOfPath + CanonicalizeQuery(str.substr(1), true, true) + kEndOfURL;
  else
    pattern = kEndOfPath + CanonicalizeQuery(str, true, true) + kEndOfURL;

  return CreateCondition(URLMatcherCondition::QUERY_EQUALS, pattern);
}

URLMatcherCondition
URLMatcherConditionFactory::CreateHostSuffixPathPrefixCondition(
    const std::string& host_suffix,
    const std::string& path_prefix) {
  return CreateCondition(
      URLMatcherCondition::HOST_SUFFIX_PATH_PREFIX,
      CanonicalizeHostSuffix(host_suffix) + kEndOfDomain + path_prefix);
}

URLMatcherCondition
URLMatcherConditionFactory::CreateHostEqualsPathPrefixCondition(
    const std::string& host,
    const std::string& path_prefix) {
  return CreateCondition(URLMatcherCondition::HOST_EQUALS_PATH_PREFIX,
                         kBeginningOfURL + CanonicalizeHostname(host) +
                             kEndOfDomain + path_prefix);
}

std::string URLMatcherConditionFactory::CanonicalizeURLForFullSearches(
    const GURL& url) const {
  GURL::Replacements replacements;
  replacements.ClearPassword();
  replacements.ClearUsername();
  replacements.ClearRef();
  // Clear port if it is implicit from scheme.
  if (url.has_port()) {
    const std::string& port = url.scheme();
    if (url::DefaultPortForScheme(port) == url.EffectiveIntPort()) {
      replacements.ClearPort();
    }
  }
  return kBeginningOfURL + url.ReplaceComponents(replacements).spec() +
         kEndOfURL;
}

static std::string CanonicalizeURLForRegexSearchesHelper(const GURL& url,
                                                         bool clear_query) {
  GURL::Replacements replacements;
  replacements.ClearPassword();
  replacements.ClearUsername();
  replacements.ClearRef();
  if (clear_query)
    replacements.ClearQuery();
  // Clear port if it is implicit from scheme.
  if (url.has_port()) {
    const std::string& port = url.scheme();
    if (url::DefaultPortForScheme(port) == url.EffectiveIntPort()) {
      replacements.ClearPort();
    }
  }
  return url.ReplaceComponents(replacements).spec();
}

std::string URLMatcherConditionFactory::CanonicalizeURLForRegexSearches(
    const GURL& url) const {
  return CanonicalizeURLForRegexSearchesHelper(url, false);
}

std::string
URLMatcherConditionFactory::CanonicalizeURLForOriginAndPathRegexSearches(
    const GURL& url) const {
  return CanonicalizeURLForRegexSearchesHelper(url, true);
}

URLMatcherCondition URLMatcherConditionFactory::CreateURLPrefixCondition(
    const std::string& prefix) {
  return CreateCondition(URLMatcherCondition::URL_PREFIX,
                         kBeginningOfURL + prefix);
}

URLMatcherCondition URLMatcherConditionFactory::CreateURLSuffixCondition(
    const std::string& suffix) {
  return CreateCondition(URLMatcherCondition::URL_SUFFIX, suffix + kEndOfURL);
}

URLMatcherCondition URLMatcherConditionFactory::CreateURLContainsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::URL_CONTAINS, str);
}

URLMatcherCondition URLMatcherConditionFactory::CreateURLEqualsCondition(
    const std::string& str) {
  return CreateCondition(URLMatcherCondition::URL_EQUALS,
                         kBeginningOfURL + str + kEndOfURL);
}

URLMatcherCondition URLMatcherConditionFactory::CreateURLMatchesCondition(
    const std::string& regex) {
  return CreateCondition(URLMatcherCondition::URL_MATCHES, regex);
}

URLMatcherCondition
URLMatcherConditionFactory::CreateOriginAndPathMatchesCondition(
    const std::string& regex) {
  return CreateCondition(URLMatcherCondition::ORIGIN_AND_PATH_MATCHES, regex);
}

void URLMatcherConditionFactory::ForgetUnusedPatterns(
    const std::set<MatcherStringPattern::ID>& used_patterns) {
  auto i = substring_pattern_singletons_.begin();
  while (i != substring_pattern_singletons_.end()) {
    if (base::Contains(used_patterns, i->first->id()))
      ++i;
    else
      substring_pattern_singletons_.erase(i++);
  }

  i = regex_pattern_singletons_.begin();
  while (i != regex_pattern_singletons_.end()) {
    if (base::Contains(used_patterns, i->first->id()))
      ++i;
    else
      regex_pattern_singletons_.erase(i++);
  }

  i = origin_and_path_regex_pattern_singletons_.begin();
  while (i != origin_and_path_regex_pattern_singletons_.end()) {
    if (base::Contains(used_patterns, i->first->id()))
      ++i;
    else
      origin_and_path_regex_pattern_singletons_.erase(i++);
  }
}

bool URLMatcherConditionFactory::IsEmpty() const {
  return substring_pattern_singletons_.empty() &&
         regex_pattern_singletons_.empty() &&
         origin_and_path_regex_pattern_singletons_.empty();
}

URLMatcherCondition URLMatcherConditionFactory::CreateCondition(
    URLMatcherCondition::Criterion criterion,
    const std::string& pattern) {
  MatcherStringPattern search_pattern(pattern, 0);
  PatternSingletons* pattern_singletons = nullptr;
  if (IsRegexCriterion(criterion))
    pattern_singletons = &regex_pattern_singletons_;
  else if (IsOriginAndPathRegexCriterion(criterion))
    pattern_singletons = &origin_and_path_regex_pattern_singletons_;
  else
    pattern_singletons = &substring_pattern_singletons_;

  auto iter = pattern_singletons->find(&search_pattern);

  if (iter != pattern_singletons->end())
    return URLMatcherCondition(criterion, iter->first);

  MatcherStringPattern* new_pattern =
      new MatcherStringPattern(pattern, GetNextID());
  (*pattern_singletons)[new_pattern] = base::WrapUnique(new_pattern);
  return URLMatcherCondition(criterion, new_pattern);
}

std::string URLMatcherConditionFactory::CanonicalizeHostSuffix(
    const std::string& suffix) const {
  if (suffix.empty())
    return ".";
  return suffix.back() == '.' ? suffix : suffix + ".";
}

std::string URLMatcherConditionFactory::CanonicalizeHostPrefix(
    const std::string& prefix) const {
  if (prefix.empty())
    return ".";
  return prefix[0] == '.' ? prefix : "." + prefix;
}

std::string URLMatcherConditionFactory::CanonicalizeHostname(
    const std::string& hostname) const {
  return CanonicalizeHostPrefix(CanonicalizeHostSuffix(hostname));
}

// This function prepares the query string by replacing query separator with a
// magic value (|kQueryComponentDelimiter|). When the boolean
// |prepend_beginning_of_query_component| is true the function prepends the
// query with the same magic. This is done to locate the start of a key value
// pair in the query string. The parameter |query| is passed by value
// intentionally, since it is locally modified.
std::string URLMatcherConditionFactory::CanonicalizeQuery(
    std::string query,
    bool prepend_beginning_of_query_component,
    bool append_end_of_query_component) const {
  for (std::string::iterator it = query.begin(); it != query.end(); ++it) {
    if (*it == kQuerySeparator)
      *it = kQueryComponentDelimiter[0];
  }
  if (prepend_beginning_of_query_component)
    query = kQueryComponentDelimiter + query;
  if (append_end_of_query_component)
    query += kQueryComponentDelimiter;
  return query;
}

base::MatcherStringPattern::ID URLMatcherConditionFactory::GetNextID() {
  id_counter_++;

  if (id_counter_ == MatcherStringPattern::kInvalidId)
    id_counter_++;

  return id_counter_;
}

bool URLMatcherConditionFactory::MatcherStringPatternPointerCompare::operator()(
    MatcherStringPattern* lhs,
    MatcherStringPattern* rhs) const {
  if (lhs == nullptr && rhs != nullptr)
    return true;
  if (lhs != nullptr && rhs != nullptr)
    return lhs->pattern() < rhs->pattern();
  // Either both are NULL or only rhs is NULL.
  return false;
}

//
// URLQueryElementMatcherCondition
//

URLQueryElementMatcherCondition::URLQueryElementMatcherCondition(
    const std::string& key,
    const std::string& value,
    QueryValueMatchType query_value_match_type,
    QueryElementType query_element_type,
    Type match_type,
    URLMatcherConditionFactory* factory) {
  match_type_ = match_type;

  if (query_element_type == ELEMENT_TYPE_KEY_VALUE) {
    key_ = kQueryComponentDelimiter + key + "=";
    value_ = value;
  } else {
    key_ = kQueryComponentDelimiter + key;
    value_ = std::string();
  }

  if (query_value_match_type == QUERY_VALUE_MATCH_EXACT)
    value_ += kQueryComponentDelimiter;

  // If |value_| is empty no need to find the |key_| and verify if the value
  // matches. Simply checking the presence of key is sufficient, which is done
  // by MATCH_ANY
  if (value_.empty())
    match_type_ = MATCH_ANY;

  URLMatcherCondition condition;
  // If |match_type_| is MATCH_ANY, then we could simply look for the
  // combination of |key_| + |value_|, which can be efficiently done by
  // SubstringMatcher
  if (match_type_ == MATCH_ANY)
    condition = factory->CreateQueryContainsCondition(key_ + value_);
  else
    condition = factory->CreateQueryContainsCondition(key_);
  string_pattern_ = condition.string_pattern();

  key_length_ = key_.length();
  value_length_ = value_.length();
}

URLQueryElementMatcherCondition::URLQueryElementMatcherCondition(
    const URLQueryElementMatcherCondition& other) = default;

URLQueryElementMatcherCondition::~URLQueryElementMatcherCondition() = default;

bool URLQueryElementMatcherCondition::operator<(
    const URLQueryElementMatcherCondition& rhs) const {
  if (match_type_ != rhs.match_type_)
    return match_type_ < rhs.match_type_;
  if (string_pattern_ != nullptr && rhs.string_pattern_ != nullptr)
    return *string_pattern_ < *rhs.string_pattern_;
  if (string_pattern_ == nullptr && rhs.string_pattern_ != nullptr)
    return true;
  // Either string_pattern_ != NULL && rhs.string_pattern_ == NULL,
  // or both are NULL.
  return false;
}

bool URLQueryElementMatcherCondition::IsMatch(
    const std::string& url_for_component_searches) const {
  switch (match_type_) {
    case MATCH_ANY: {
      // For MATCH_ANY, no additional verification step is needed. We can trust
      // the SubstringMatcher to do the verification.
      return true;
    }
    case MATCH_ALL: {
      size_t start = 0;
      int found = 0;
      size_t offset;
      while ((offset = url_for_component_searches.find(key_, start)) !=
             std::string::npos) {
        if (url_for_component_searches.compare(offset + key_length_,
                                               value_length_, value_) != 0) {
          return false;
        } else {
          ++found;
        }
        start = offset + key_length_ + value_length_ - 1;
      }
      return !!found;
    }
    case MATCH_FIRST: {
      size_t offset = url_for_component_searches.find(key_);
      return url_for_component_searches.compare(offset + key_length_,
                                                value_length_, value_) == 0;
    }
    case MATCH_LAST: {
      size_t offset = url_for_component_searches.rfind(key_);
      return url_for_component_searches.compare(offset + key_length_,
                                                value_length_, value_) == 0;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

//
// URLMatcherSchemeFilter
//

URLMatcherSchemeFilter::URLMatcherSchemeFilter(const std::string& filter)
    : filters_(1) {
  filters_.push_back(filter);
}

URLMatcherSchemeFilter::URLMatcherSchemeFilter(
    const std::vector<std::string>& filters)
    : filters_(filters) {}

URLMatcherSchemeFilter::~URLMatcherSchemeFilter() = default;

bool URLMatcherSchemeFilter::IsMatch(const GURL& url) const {
  return base::Contains(filters_, url.scheme());
}

//
// URLMatcherPortFilter
//

URLMatcherPortFilter::URLMatcherPortFilter(
    const std::vector<URLMatcherPortFilter::Range>& ranges)
    : ranges_(ranges) {}

URLMatcherPortFilter::~URLMatcherPortFilter() = default;

bool URLMatcherPortFilter::IsMatch(const GURL& url) const {
  int port = url.EffectiveIntPort();
  for (auto i = ranges_.begin(); i != ranges_.end(); ++i) {
    if (i->first <= port && port <= i->second)
      return true;
  }
  return false;
}

// static
URLMatcherPortFilter::Range URLMatcherPortFilter::CreateRange(int from,
                                                              int to) {
  return Range(from, to);
}

// static
URLMatcherPortFilter::Range URLMatcherPortFilter::CreateRange(int port) {
  return Range(port, port);
}

//
// URLMatcherCidrBlockFilter
//

URLMatcherCidrBlockFilter::URLMatcherCidrBlockFilter(
    std::vector<URLMatcherCidrBlockFilter::CidrBlock>&& cidr_blocks)
    : cidr_blocks_(std::move(cidr_blocks)) {}

URLMatcherCidrBlockFilter::~URLMatcherCidrBlockFilter() = default;

bool URLMatcherCidrBlockFilter::IsMatch(const GURL& url) const {
  // Make sure host is an IP address.
  if (!url.HostIsIPAddress()) {
    return false;
  }

  // Parse the input IP literal to a number.
  net::IPAddress ip_address;
  if (!ip_address.AssignFromIPLiteral(url.HostNoBracketsPiece())) {
    return false;
  }

  return base::ranges::any_of(cidr_blocks_, [&ip_address](
                                                const CidrBlock& block) {
    return net::IPAddressMatchesPrefix(ip_address, block.first, block.second);
  });
}

// static
base::expected<URLMatcherCidrBlockFilter::CidrBlock, std::string>
URLMatcherCidrBlockFilter::CreateCidrBlock(const std::string& entry) {
  net::IPAddress ip_address;
  size_t prefix_length_in_bits = 0;
  if (!net::ParseCIDRBlock(entry, &ip_address, &prefix_length_in_bits)) {
    return base::unexpected("Failed parsing CIDR");
  }

  return CidrBlock(ip_address, prefix_length_in_bits);
}

//
// URLMatcherConditionSet
//

URLMatcherConditionSet::~URLMatcherConditionSet() = default;

URLMatcherConditionSet::URLMatcherConditionSet(
    base::MatcherStringPattern::ID id,
    const Conditions& conditions)
    : id_(id), conditions_(conditions) {}

URLMatcherConditionSet::URLMatcherConditionSet(
    base::MatcherStringPattern::ID id,
    const Conditions& conditions,
    std::unique_ptr<URLMatcherSchemeFilter> scheme_filter,
    std::unique_ptr<URLMatcherPortFilter> port_filter,
    std::unique_ptr<URLMatcherCidrBlockFilter> cidr_block_filter)
    : id_(id),
      conditions_(conditions),
      scheme_filter_(std::move(scheme_filter)),
      port_filter_(std::move(port_filter)),
      cidr_block_filter_(std::move(cidr_block_filter)) {}

URLMatcherConditionSet::URLMatcherConditionSet(
    base::MatcherStringPattern::ID id,
    const Conditions& conditions,
    const QueryConditions& query_conditions,
    std::unique_ptr<URLMatcherSchemeFilter> scheme_filter,
    std::unique_ptr<URLMatcherPortFilter> port_filter)
    : id_(id),
      conditions_(conditions),
      query_conditions_(query_conditions),
      scheme_filter_(std::move(scheme_filter)),
      port_filter_(std::move(port_filter)) {}

bool URLMatcherConditionSet::IsMatch(
    const std::set<MatcherStringPattern::ID>& matching_patterns,
    const GURL& url) const {
  return IsMatch(matching_patterns, url, std::string());
}

bool URLMatcherConditionSet::IsMatch(
    const std::set<MatcherStringPattern::ID>& matching_patterns,
    const GURL& url,
    const std::string& url_for_component_searches) const {
  for (auto i = conditions_.begin(); i != conditions_.end(); ++i) {
    if (!i->IsMatch(matching_patterns, url))
      return false;
  }
  if (scheme_filter_.get() && !scheme_filter_->IsMatch(url))
    return false;
  if (port_filter_.get() && !port_filter_->IsMatch(url))
    return false;
  if (cidr_block_filter_.get() && !cidr_block_filter_->IsMatch(url)) {
    return false;
  }
  if (query_conditions_.empty())
    return true;
  // The loop is duplicated below for performance reasons. If not all query
  // elements are found, no need to verify match that is expected to take more
  // cycles.
  for (auto i = query_conditions_.begin(); i != query_conditions_.end(); ++i) {
    if (!base::Contains(matching_patterns, i->string_pattern()->id()))
      return false;
  }
  for (auto i = query_conditions_.begin(); i != query_conditions_.end(); ++i) {
    if (!i->IsMatch(url_for_component_searches))
      return false;
  }
  return true;
}

//
// URLMatcher
//

URLMatcher::URLMatcher() = default;

URLMatcher::~URLMatcher() = default;

void URLMatcher::AddConditionSets(
    const URLMatcherConditionSet::Vector& condition_sets) {
  for (auto i = condition_sets.begin(); i != condition_sets.end(); ++i) {
    DCHECK(url_matcher_condition_sets_.find((*i)->id()) ==
           url_matcher_condition_sets_.end());
    url_matcher_condition_sets_[(*i)->id()] = *i;
  }
  UpdateInternalDatastructures();
}

void URLMatcher::RemoveConditionSets(
    const std::vector<base::MatcherStringPattern::ID>& condition_set_ids) {
  for (auto id : condition_set_ids) {
    DCHECK(url_matcher_condition_sets_.find(id) !=
           url_matcher_condition_sets_.end());
    url_matcher_condition_sets_.erase(id);
  }
  UpdateInternalDatastructures();
}

void URLMatcher::ClearUnusedConditionSets() {
  UpdateConditionFactory();
}

std::set<base::MatcherStringPattern::ID> URLMatcher::MatchURL(
    const GURL& url) const {
  // Find all IDs of MatcherStringPatterns that match |url|.
  // See URLMatcherConditionFactory for the canonicalization of URLs and the
  // distinction between full url searches and url component searches.
  std::set<MatcherStringPattern::ID> matches;
  std::string url_for_component_searches;

  if (!IsMatcherEmpty(full_url_matcher_)) {
    full_url_matcher_->Match(
        condition_factory_.CanonicalizeURLForFullSearches(url), &matches);
  }
  if (!IsMatcherEmpty(url_component_matcher_)) {
    url_for_component_searches =
        condition_factory_.CanonicalizeURLForComponentSearches(url);
    url_component_matcher_->Match(url_for_component_searches, &matches);
  }
  if (!regex_set_matcher_.IsEmpty()) {
    regex_set_matcher_.Match(
        condition_factory_.CanonicalizeURLForRegexSearches(url), &matches);
  }
  if (!origin_and_path_regex_set_matcher_.IsEmpty()) {
    origin_and_path_regex_set_matcher_.Match(
        condition_factory_.CanonicalizeURLForOriginAndPathRegexSearches(url),
        &matches);
  }

  // Calculate all URLMatcherConditionSets for which all URLMatcherConditions
  // were fulfilled.
  std::set<base::MatcherStringPattern::ID> result;
  for (auto i = matches.begin(); i != matches.end(); ++i) {
    // For each URLMatcherConditionSet there is exactly one condition
    // registered in substring_match_triggers_. This means that the following
    // logic tests each URLMatcherConditionSet exactly once if it can be
    // completely fulfilled.
    auto triggered_condition_sets_iter = substring_match_triggers_.find(*i);
    if (triggered_condition_sets_iter == substring_match_triggers_.end())
      continue;  // Not all substring matches are triggers for a condition set.
    const std::set<base::MatcherStringPattern::ID>& condition_sets =
        triggered_condition_sets_iter->second;
    for (auto j = condition_sets.begin(); j != condition_sets.end(); ++j) {
      auto condition_set_iter = url_matcher_condition_sets_.find(*j);
      // Expensive: DCHECK as this is a tight loop.
      DCHECK(condition_set_iter != url_matcher_condition_sets_.end());
      if (condition_set_iter->second->IsMatch(matches, url,
                                              url_for_component_searches))
        result.insert(*j);
    }
  }

  return result;
}

bool URLMatcher::IsEmpty() const {
  return condition_factory_.IsEmpty() && url_matcher_condition_sets_.empty() &&
         substring_match_triggers_.empty() &&
         IsMatcherEmpty(full_url_matcher_) &&
         IsMatcherEmpty(url_component_matcher_) &&
         regex_set_matcher_.IsEmpty() &&
         origin_and_path_regex_set_matcher_.IsEmpty();
}

void URLMatcher::UpdateSubstringSetMatcher(bool full_url_conditions) {
  // The purpose of |full_url_conditions| is just that we need to execute
  // the same logic once for Full URL searches and once for URL Component
  // searches (see URLMatcherConditionFactory).

  // Determine which patterns need to be registered when this function
  // terminates.
  std::set<const MatcherStringPattern*> new_patterns;
  for (URLMatcherConditionSets::const_iterator condition_set_iter =
           url_matcher_condition_sets_.begin();
       condition_set_iter != url_matcher_condition_sets_.end();
       ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    for (auto condition_iter = conditions.begin();
         condition_iter != conditions.end(); ++condition_iter) {
      // If we are called to process Full URL searches, ignore others, and
      // vice versa. (Regex conditions are updated in UpdateRegexSetMatcher.)
      if (!condition_iter->IsRegexCondition() &&
          !condition_iter->IsOriginAndPathRegexCondition() &&
          full_url_conditions == condition_iter->IsFullURLCondition())
        new_patterns.insert(condition_iter->string_pattern());
    }

    if (full_url_conditions)
      continue;

    const URLMatcherConditionSet::QueryConditions& query_conditions =
        condition_set_iter->second->query_conditions();
    for (auto query_condition_iter = query_conditions.begin();
         query_condition_iter != query_conditions.end();
         ++query_condition_iter) {
      new_patterns.insert(query_condition_iter->string_pattern());
    }
  }

  // Update the SubstringSetMatcher.
  std::unique_ptr<SubstringSetMatcher>& url_matcher =
      full_url_conditions ? full_url_matcher_ : url_component_matcher_;

  url_matcher = std::make_unique<SubstringSetMatcher>();
  bool success = url_matcher->Build(std::vector<const MatcherStringPattern*>(
      new_patterns.begin(), new_patterns.end()));
  CHECK(success);
}

void URLMatcher::UpdateRegexSetMatcher() {
  std::vector<const MatcherStringPattern*> new_patterns;
  std::vector<const MatcherStringPattern*> new_origin_and_path_patterns;

  for (URLMatcherConditionSets::const_iterator condition_set_iter =
           url_matcher_condition_sets_.begin();
       condition_set_iter != url_matcher_condition_sets_.end();
       ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    for (auto condition_iter = conditions.begin();
         condition_iter != conditions.end(); ++condition_iter) {
      if (condition_iter->IsRegexCondition()) {
        new_patterns.push_back(condition_iter->string_pattern());
      } else if (condition_iter->IsOriginAndPathRegexCondition()) {
        new_origin_and_path_patterns.push_back(
            condition_iter->string_pattern());
      }
    }
  }

  // Start over from scratch. We can't really do better than this, since the
  // FilteredRE2 backend doesn't support incremental updates.
  regex_set_matcher_.ClearPatterns();
  regex_set_matcher_.AddPatterns(new_patterns);
  origin_and_path_regex_set_matcher_.ClearPatterns();
  origin_and_path_regex_set_matcher_.AddPatterns(new_origin_and_path_patterns);
}

void URLMatcher::UpdateTriggers() {
  // Count substring pattern frequencies.
  std::map<MatcherStringPattern::ID, size_t> substring_pattern_frequencies;
  for (URLMatcherConditionSets::const_iterator condition_set_iter =
           url_matcher_condition_sets_.begin();
       condition_set_iter != url_matcher_condition_sets_.end();
       ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    for (auto condition_iter = conditions.begin();
         condition_iter != conditions.end(); ++condition_iter) {
      const MatcherStringPattern* pattern = condition_iter->string_pattern();
      substring_pattern_frequencies[pattern->id()]++;
    }

    const URLMatcherConditionSet::QueryConditions& query_conditions =
        condition_set_iter->second->query_conditions();
    for (auto query_condition_iter = query_conditions.begin();
         query_condition_iter != query_conditions.end();
         ++query_condition_iter) {
      const MatcherStringPattern* pattern =
          query_condition_iter->string_pattern();
      substring_pattern_frequencies[pattern->id()]++;
    }
  }

  // Update trigger conditions: Determine for each URLMatcherConditionSet which
  // URLMatcherCondition contains a MatcherStringPattern that occurs least
  // frequently in this URLMatcher. We assume that this condition is very
  // specific and occurs rarely in URLs. If a match occurs for this
  // URLMatcherCondition, we want to test all other URLMatcherCondition in the
  // respective URLMatcherConditionSet as well to see whether the entire
  // URLMatcherConditionSet is considered matching.
  substring_match_triggers_.clear();
  for (URLMatcherConditionSets::const_iterator condition_set_iter =
           url_matcher_condition_sets_.begin();
       condition_set_iter != url_matcher_condition_sets_.end();
       ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    if (conditions.empty())
      continue;
    auto condition_iter = conditions.begin();
    MatcherStringPattern::ID trigger = condition_iter->string_pattern()->id();
    // We skip the first element in the following loop.
    ++condition_iter;
    for (; condition_iter != conditions.end(); ++condition_iter) {
      MatcherStringPattern::ID current_id =
          condition_iter->string_pattern()->id();
      if (substring_pattern_frequencies[trigger] >
          substring_pattern_frequencies[current_id]) {
        trigger = current_id;
      }
    }

    const URLMatcherConditionSet::QueryConditions& query_conditions =
        condition_set_iter->second->query_conditions();
    for (auto query_condition_iter = query_conditions.begin();
         query_condition_iter != query_conditions.end();
         ++query_condition_iter) {
      MatcherStringPattern::ID current_id =
          query_condition_iter->string_pattern()->id();
      if (substring_pattern_frequencies[trigger] >
          substring_pattern_frequencies[current_id]) {
        trigger = current_id;
      }
    }

    substring_match_triggers_[trigger].insert(condition_set_iter->second->id());
  }
}

void URLMatcher::UpdateConditionFactory() {
  std::set<MatcherStringPattern::ID> used_patterns;
  for (URLMatcherConditionSets::const_iterator condition_set_iter =
           url_matcher_condition_sets_.begin();
       condition_set_iter != url_matcher_condition_sets_.end();
       ++condition_set_iter) {
    const URLMatcherConditionSet::Conditions& conditions =
        condition_set_iter->second->conditions();
    for (auto condition_iter = conditions.begin();
         condition_iter != conditions.end(); ++condition_iter) {
      used_patterns.insert(condition_iter->string_pattern()->id());
    }
    const URLMatcherConditionSet::QueryConditions& query_conditions =
        condition_set_iter->second->query_conditions();
    for (auto query_condition_iter = query_conditions.begin();
         query_condition_iter != query_conditions.end();
         ++query_condition_iter) {
      used_patterns.insert(query_condition_iter->string_pattern()->id());
    }
  }
  condition_factory_.ForgetUnusedPatterns(used_patterns);
}

void URLMatcher::UpdateInternalDatastructures() {
  UpdateSubstringSetMatcher(false);
  UpdateSubstringSetMatcher(true);
  UpdateRegexSetMatcher();
  UpdateTriggers();
  UpdateConditionFactory();
}

}  // namespace url_matcher
