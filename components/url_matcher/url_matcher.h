// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_MATCHER_URL_MATCHER_H_
#define COMPONENTS_URL_MATCHER_URL_MATCHER_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/substring_set_matcher/substring_set_matcher.h"
#include "base/types/expected.h"
#include "components/url_matcher/regex_set_matcher.h"
#include "components/url_matcher/url_matcher_export.h"
#include "net/base/ip_address.h"

class GURL;

namespace url_matcher {

// This class represents a single URL matching condition, e.g. a match on the
// host suffix or the containment of a string in the query component of a GURL.
//
// The difference from a simple MatcherStringPattern is that this also supports
// checking whether the {Host, Path, Query} of a URL contains a string. The
// reduction of URL matching conditions to MatcherStringPatterns conducted by
// URLMatcherConditionFactory is not capable of expressing that alone.
//
// Also supported is matching regular expressions against the URL (URL_MATCHES).
class URL_MATCHER_EXPORT URLMatcherCondition {
 public:
  enum Criterion {
    HOST_PREFIX,
    HOST_SUFFIX,
    HOST_CONTAINS,
    HOST_EQUALS,
    PATH_PREFIX,
    PATH_SUFFIX,
    PATH_CONTAINS,
    PATH_EQUALS,
    QUERY_PREFIX,
    QUERY_SUFFIX,
    QUERY_CONTAINS,
    QUERY_EQUALS,
    HOST_SUFFIX_PATH_PREFIX,
    HOST_EQUALS_PATH_PREFIX,
    URL_PREFIX,
    URL_SUFFIX,
    URL_CONTAINS,
    URL_EQUALS,
    URL_MATCHES,
    ORIGIN_AND_PATH_MATCHES,  // Matches the URL minus its query string.
  };

  URLMatcherCondition();
  ~URLMatcherCondition();
  URLMatcherCondition(Criterion criterion,
                      const base::MatcherStringPattern* substring_pattern);

  bool operator<(const URLMatcherCondition& rhs) const;

  Criterion criterion() const { return criterion_; }
  const base::MatcherStringPattern* string_pattern() const {
    return string_pattern_;
  }

  // Returns whether this URLMatcherCondition needs to be executed on a
  // full URL rather than the individual components (see
  // URLMatcherConditionFactory).
  bool IsFullURLCondition() const;

  // Returns whether this URLMatcherCondition is a regular expression to be
  // handled by a regex matcher instead of a substring matcher.
  bool IsRegexCondition() const;

  // Returns whether this URLMatcherCondition is a regular expression that shall
  // be evaluated on the URL without the query parameter.
  bool IsOriginAndPathRegexCondition() const;

  // Returns whether this condition is fulfilled according to
  // |matching_patterns| and |url|.
  bool IsMatch(
      const std::set<base::MatcherStringPattern::ID>& matching_patterns,
      const GURL& url) const;

 private:
  // |criterion_| and |string_pattern_| describe together what property a URL
  // needs to fulfill to be considered a match.
  Criterion criterion_;

  // This is the MatcherStringPattern that is used in a SubstringSetMatcher.
  raw_ptr<const base::MatcherStringPattern, DanglingUntriaged> string_pattern_;
};

// Class to map the problem of finding {host, path, query} {prefixes, suffixes,
// containments, and equality} in GURLs to the substring matching problem.
//
// Say, you want to check whether the path of a URL starts with "/index.html".
// This class preprocesses a URL like "www.google.com/index.html" into something
// like "www.google.com|/index.html". After preprocessing, you can search for
// "|/index.html" in the string and see that this candidate URL actually has
// a path that starts with "/index.html". On the contrary,
// "www.google.com/images/index.html" would be normalized to
// "www.google.com|/images/index.html". It is easy to see that it contains
// "/index.html" but the path of the URL does not start with "/index.html".
//
// This preprocessing is important if you want to match a URL against many
// patterns because it reduces the matching to a "discover all substrings
// of a dictionary in a text" problem, which can be solved very efficiently
// by the Aho-Corasick algorithm.
//
// IMPORTANT: The URLMatcherConditionFactory owns the MatcherStringPattern
// referenced by created URLMatcherConditions. Therefore, it must outlive
// all created URLMatcherCondition and the SubstringSetMatcher.
class URL_MATCHER_EXPORT URLMatcherConditionFactory {
 public:
  URLMatcherConditionFactory();

  URLMatcherConditionFactory(const URLMatcherConditionFactory&) = delete;
  URLMatcherConditionFactory& operator=(const URLMatcherConditionFactory&) =
      delete;

  ~URLMatcherConditionFactory();

  // Canonicalizes a URL for "Create{Host,Path,Query}*Condition" searches.
  std::string CanonicalizeURLForComponentSearches(const GURL& url) const;

  // Factory methods for various condition types.
  //
  // Note that these methods fill the pattern_singletons_. If you create
  // conditions and don't register them to a URLMatcher, they will continue to
  // consume memory. You need to call ForgetUnusedPatterns() or
  // URLMatcher::ClearUnusedConditionSets() in this case.
  URLMatcherCondition CreateHostPrefixCondition(const std::string& prefix);
  URLMatcherCondition CreateHostSuffixCondition(const std::string& suffix);
  URLMatcherCondition CreateHostContainsCondition(const std::string& str);
  URLMatcherCondition CreateHostEqualsCondition(const std::string& str);

  URLMatcherCondition CreatePathPrefixCondition(const std::string& prefix);
  URLMatcherCondition CreatePathSuffixCondition(const std::string& suffix);
  URLMatcherCondition CreatePathContainsCondition(const std::string& str);
  URLMatcherCondition CreatePathEqualsCondition(const std::string& str);

  URLMatcherCondition CreateQueryPrefixCondition(const std::string& prefix);
  URLMatcherCondition CreateQuerySuffixCondition(const std::string& suffix);
  URLMatcherCondition CreateQueryContainsCondition(const std::string& str);
  URLMatcherCondition CreateQueryEqualsCondition(const std::string& str);

  // This covers the common case, where you don't care whether a domain
  // "foobar.com" is expressed as "foobar.com" or "www.foobar.com", and it
  // should be followed by a given |path_prefix|.
  URLMatcherCondition CreateHostSuffixPathPrefixCondition(
      const std::string& host_suffix,
      const std::string& path_prefix);
  URLMatcherCondition CreateHostEqualsPathPrefixCondition(
      const std::string& host,
      const std::string& path_prefix);

  // Canonicalizes a URL for "CreateURL*Condition" searches.
  std::string CanonicalizeURLForFullSearches(const GURL& url) const;

  // Canonicalizes a URL for "CreateURLMatchesCondition" searches.
  std::string CanonicalizeURLForRegexSearches(const GURL& url) const;
  // Canonicalizes a URL for "CreateOriginAndPathMatchesCondition" searches.
  std::string CanonicalizeURLForOriginAndPathRegexSearches(
      const GURL& url) const;

  URLMatcherCondition CreateURLPrefixCondition(const std::string& prefix);
  URLMatcherCondition CreateURLSuffixCondition(const std::string& suffix);
  URLMatcherCondition CreateURLContainsCondition(const std::string& str);
  URLMatcherCondition CreateURLEqualsCondition(const std::string& str);

  URLMatcherCondition CreateURLMatchesCondition(const std::string& regex);
  URLMatcherCondition CreateOriginAndPathMatchesCondition(
      const std::string& regex);

  // Removes all patterns from |pattern_singletons_| that are not listed in
  // |used_patterns|. These patterns are not referenced any more and get
  // freed.
  void ForgetUnusedPatterns(
      const std::set<base::MatcherStringPattern::ID>& used_patterns);

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

 private:
  // Creates a URLMatcherCondition according to the parameters passed.
  // The URLMatcherCondition will refer to a MatcherStringPattern that is
  // owned by |pattern_singletons_|.
  URLMatcherCondition CreateCondition(URLMatcherCondition::Criterion criterion,
                                      const std::string& pattern);

  // Prepends a "." to the prefix if it does not start with one.
  std::string CanonicalizeHostPrefix(const std::string& prefix) const;
  // Appends a "." to the hostname if it does not start with one.
  std::string CanonicalizeHostSuffix(const std::string& suffix) const;
  // Adds "." to either side of the hostname if not present yet.
  std::string CanonicalizeHostname(const std::string& hostname) const;

  // Convert the query string to canonical form suitable for key token search.
  std::string CanonicalizeQuery(std::string query,
                                bool prepend_beginning_of_query_component,
                                bool append_end_of_query_component) const;

  // Return the next MatcherStringPattern id to use.
  base::MatcherStringPattern::ID GetNextID();

  // Counter that ensures that all created MatcherStringPatterns have unique
  // IDs. Note that substring patterns and regex patterns will use different
  // IDs.
  base::MatcherStringPattern::ID id_counter_ = 0;

  // This comparison considers only the pattern() value of the
  // MatcherStringPatterns.
  struct MatcherStringPatternPointerCompare {
    bool operator()(base::MatcherStringPattern* lhs,
                    base::MatcherStringPattern* rhs) const;
  };
  // Set to ensure that we generate only one MatcherStringPattern for each
  // content of MatcherStringPattern::pattern().
  using PatternSingletons =
      std::map<base::MatcherStringPattern*,
               std::unique_ptr<base::MatcherStringPattern>,
               MatcherStringPatternPointerCompare>;
  PatternSingletons substring_pattern_singletons_;
  PatternSingletons regex_pattern_singletons_;
  PatternSingletons origin_and_path_regex_pattern_singletons_;
};

// This class represents a single URL query matching condition. The query
// matching is done as a search for a key and optionally a value.
// The matching makes use of CanonicalizeURLForComponentSearches to ensure that
// the key starts and ends (optionally) with the right marker.
class URL_MATCHER_EXPORT URLQueryElementMatcherCondition {
 public:
  // Multiple occurrences of the same key can happen in a URL query. The type
  // ensures that every (MATCH_ALL), any (MATCH_ANY), first (MATCH_FIRST) or
  // last (MATCH_LAST) instance of the key occurrence matches the value.
  enum Type { MATCH_ANY, MATCH_FIRST, MATCH_LAST, MATCH_ALL };

  // Allows the match to be exact (QUERY_VALUE_MATCH_EXACT, starts and ends with
  // a delimiter or a border) or simply a prefix (QUERY_VALUE_MATCH_PREFIX,
  // starts with a delimiter or a border).
  enum QueryValueMatchType {
    QUERY_VALUE_MATCH_EXACT,
    QUERY_VALUE_MATCH_PREFIX
  };

  // Used to indicate if the query parameter is of type &key=value&
  // (ELEMENT_TYPE_KEY_VALUE) or simply &key& (ELEMENT_TYPE_KEY).
  enum QueryElementType { ELEMENT_TYPE_KEY_VALUE, ELEMENT_TYPE_KEY };

  URLQueryElementMatcherCondition(const std::string& key,
                                  const std::string& value,
                                  QueryValueMatchType query_value_match_type,
                                  QueryElementType query_element_type,
                                  Type match_type,
                                  URLMatcherConditionFactory* factory);
  URLQueryElementMatcherCondition(const URLQueryElementMatcherCondition& other);
  ~URLQueryElementMatcherCondition();

  bool operator<(const URLQueryElementMatcherCondition& rhs) const;

  // Returns whether the URL query satisfies the key value constraint.
  bool IsMatch(const std::string& canonical_url_query) const;

  const base::MatcherStringPattern* string_pattern() const {
    return string_pattern_;
  }

 private:
  Type match_type_;
  std::string key_;
  std::string value_;
  size_t key_length_;
  size_t value_length_;
  raw_ptr<const base::MatcherStringPattern> string_pattern_;
};

// This class represents a filter for the URL scheme to be hooked up into a
// URLMatcherConditionSet.
class URL_MATCHER_EXPORT URLMatcherSchemeFilter {
 public:
  explicit URLMatcherSchemeFilter(const std::string& filter);
  explicit URLMatcherSchemeFilter(const std::vector<std::string>& filters);

  URLMatcherSchemeFilter(const URLMatcherSchemeFilter&) = delete;
  URLMatcherSchemeFilter& operator=(const URLMatcherSchemeFilter&) = delete;

  ~URLMatcherSchemeFilter();
  bool IsMatch(const GURL& url) const;

 private:
  std::vector<std::string> filters_;
};

// This class represents a filter for port numbers to be hooked up into a
// URLMatcherConditionSet.
class URL_MATCHER_EXPORT URLMatcherPortFilter {
 public:
  // Boundaries of a port range (both ends are included).
  typedef std::pair<int, int> Range;
  explicit URLMatcherPortFilter(const std::vector<Range>& ranges);

  URLMatcherPortFilter(const URLMatcherPortFilter&) = delete;
  URLMatcherPortFilter& operator=(const URLMatcherPortFilter&) = delete;

  ~URLMatcherPortFilter();
  bool IsMatch(const GURL& url) const;

  // Creates a port range [from, to]; both ends are included.
  static Range CreateRange(int from, int to);
  // Creates a port range containing a single port.
  static Range CreateRange(int port);

 private:
  std::vector<Range> ranges_;
};

// This class represents a filter for CIDR blocks to be hooked up into a
// URLMatcherConditionSet.
class URL_MATCHER_EXPORT URLMatcherCidrBlockFilter {
 public:
  // IP range in CIDR notation.
  using CidrBlock = std::pair<net::IPAddress, size_t>;
  explicit URLMatcherCidrBlockFilter(std::vector<CidrBlock>&& cidr_blocks);

  URLMatcherCidrBlockFilter(const URLMatcherCidrBlockFilter&) = delete;
  URLMatcherCidrBlockFilter& operator=(const URLMatcherCidrBlockFilter&) =
      delete;

  ~URLMatcherCidrBlockFilter();
  bool IsMatch(const GURL& url) const;

  // Creates a CIDR block.
  static base::expected<URLMatcherCidrBlockFilter::CidrBlock, std::string>
  CreateCidrBlock(const std::string& entry);

 private:
  std::vector<CidrBlock> cidr_blocks_;
};

// This class represents a set of conditions that all need to match on a
// given URL in order to be considered a match.
class URL_MATCHER_EXPORT URLMatcherConditionSet
    : public base::RefCounted<URLMatcherConditionSet> {
 public:
  typedef std::set<URLMatcherCondition> Conditions;
  typedef std::set<URLQueryElementMatcherCondition> QueryConditions;
  typedef std::vector<scoped_refptr<URLMatcherConditionSet>> Vector;

  // Matches if all conditions in |conditions| are fulfilled.
  URLMatcherConditionSet(base::MatcherStringPattern::ID id,
                         const Conditions& conditions);

  // Matches if all conditions in |conditions|, |scheme_filter|,
  // |port_filter| and |cidr_block_filter| are fulfilled.
  // |scheme_filter|, |port_filter| and |cidr_block_filter| may be NULL,
  // in which case, no restrictions are imposed on the scheme/port/cidr block of
  // a URL.
  URLMatcherConditionSet(
      base::MatcherStringPattern::ID id,
      const Conditions& conditions,
      std::unique_ptr<URLMatcherSchemeFilter> scheme_filter,
      std::unique_ptr<URLMatcherPortFilter> port_filter,
      std::unique_ptr<URLMatcherCidrBlockFilter> cidr_block_filter);

  // Matches if all conditions in |conditions|, |query_conditions|,
  // |scheme_filter| and |port_filter| are fulfilled. |scheme_filter| and
  // |port_filter| may be NULL, in which case, no restrictions are imposed on
  // the scheme/port of a URL.
  URLMatcherConditionSet(base::MatcherStringPattern::ID id,
                         const Conditions& conditions,
                         const QueryConditions& query_conditions,
                         std::unique_ptr<URLMatcherSchemeFilter> scheme_filter,
                         std::unique_ptr<URLMatcherPortFilter> port_filter);

  URLMatcherConditionSet(const URLMatcherConditionSet&) = delete;
  URLMatcherConditionSet& operator=(const URLMatcherConditionSet&) = delete;

  base::MatcherStringPattern::ID id() const { return id_; }
  const Conditions& conditions() const { return conditions_; }
  const QueryConditions& query_conditions() const { return query_conditions_; }

  bool IsMatch(
      const std::set<base::MatcherStringPattern::ID>& matching_patterns,
      const GURL& url) const;

  bool IsMatch(
      const std::set<base::MatcherStringPattern::ID>& matching_patterns,
      const GURL& url,
      const std::string& url_for_component_searches) const;

 private:
  friend class base::RefCounted<URLMatcherConditionSet>;
  ~URLMatcherConditionSet();
  base::MatcherStringPattern::ID id_ = 0;
  Conditions conditions_;
  QueryConditions query_conditions_;
  std::unique_ptr<URLMatcherSchemeFilter> scheme_filter_;
  std::unique_ptr<URLMatcherPortFilter> port_filter_;
  std::unique_ptr<URLMatcherCidrBlockFilter> cidr_block_filter_;
};

// This class allows matching one URL against a large set of
// URLMatcherConditionSets at the same time.
class URL_MATCHER_EXPORT URLMatcher {
 public:
  URLMatcher();

  URLMatcher(const URLMatcher&) = delete;
  URLMatcher& operator=(const URLMatcher&) = delete;

  ~URLMatcher();

  // Adds new URLMatcherConditionSet to this URL Matcher. Each condition set
  // must have a unique ID.
  // This is an expensive operation as it triggers pre-calculations on the
  // currently registered condition sets. Do not call this operation many
  // times with a single condition set in each call.
  void AddConditionSets(const URLMatcherConditionSet::Vector& condition_sets);

  // Removes the listed condition sets. All |condition_set_ids| must be
  // currently registered. This function should be called with large batches
  // of |condition_set_ids| at a time to improve performance.
  void RemoveConditionSets(
      const std::vector<base::MatcherStringPattern::ID>& condition_set_ids);

  // Removes all unused condition sets from the ConditionFactory.
  void ClearUnusedConditionSets();

  // Returns the IDs of all URLMatcherConditionSet that match to this |url|.
  std::set<base::MatcherStringPattern::ID> MatchURL(const GURL& url) const;

  // Returns the URLMatcherConditionFactory that must be used to create
  // URLMatcherConditionSets for this URLMatcher.
  URLMatcherConditionFactory* condition_factory() {
    return &condition_factory_;
  }

  // Returns true if this object retains no allocated data. Only for debugging.
  bool IsEmpty() const;

 private:
  void UpdateSubstringSetMatcher(bool full_url_conditions);
  void UpdateRegexSetMatcher();
  void UpdateTriggers();
  void UpdateConditionFactory();
  void UpdateInternalDatastructures();

  URLMatcherConditionFactory condition_factory_;

  // Maps the ID of a URLMatcherConditionSet to the respective
  // URLMatcherConditionSet.
  typedef std::map<base::MatcherStringPattern::ID,
                   scoped_refptr<URLMatcherConditionSet>>
      URLMatcherConditionSets;
  URLMatcherConditionSets url_matcher_condition_sets_;

  // Maps a MatcherStringPattern ID to the URLMatcherConditions that need to
  // be triggered in case of a MatcherStringPattern match.
  typedef std::map<base::MatcherStringPattern::ID,
                   std::set<base::MatcherStringPattern::ID>>
      MatcherStringPatternTriggers;
  MatcherStringPatternTriggers substring_match_triggers_;

  std::unique_ptr<base::SubstringSetMatcher> full_url_matcher_;
  std::unique_ptr<base::SubstringSetMatcher> url_component_matcher_;
  RegexSetMatcher regex_set_matcher_;
  RegexSetMatcher origin_and_path_regex_set_matcher_;
};

}  // namespace url_matcher

#endif  // COMPONENTS_URL_MATCHER_URL_MATCHER_H_
