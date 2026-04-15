// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains definitions of data structures needed for representing
// filtering/hiding rules as parsed from EasyList. See the following links for
// detailed explanation on available rule types and their syntax:
//  https://adblockplus.org/en/filters
//  https://adblockplus.org/en/filter-cheatsheet
// For out-of-documentation options see the following:
//  https://adblockplus.org/forum/viewtopic.php?t=9353
//  https://adblockplus.org/development-builds/experimental-pop-up-blocking-support
//  https://adblockplus.org/development-builds/new-filter-options-generichide-and-genericblock
//
// TODO(pkalinnikov): Consider removing these classes, leaving only the
// corresponding protobuf structures.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULE_PARSER_RULE_H_
#define COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULE_PARSER_RULE_H_

#include <ostream>
#include <string>
#include <vector>

#include "components/subresource_filter/tools/rule_parser/rule_options.h"
#include "components/url_pattern_index/proto/rules.pb.h"

namespace subresource_filter {

using UrlPatternType = url_pattern_index::proto::UrlPatternType;
using RuleType = url_pattern_index::proto::RuleType;
using AnchorType = url_pattern_index::proto::AnchorType;

// Represents the three values in a kind of tri-state logic.
enum class TriState {
  DONT_CARE = 0,
  YES = 1,
  NO = 2,
};

// A single URL filtering rule as parsed from EasyList.
// TODO(pkalinnikov): Add 'sitekey', 'collapse', and 'donottrack' options.
struct UrlRule {
  // Constructs a default empty blocklist rule. That means URL pattern is empty,
  // not case sensitive and not anchored, domain list is empty, and the rule is
  // associated with all ElementType's (except POPUP), and none of the
  // ActivationType's.
  UrlRule();

  UrlRule(const UrlRule&);
  ~UrlRule();
  UrlRule& operator=(const UrlRule&);

  bool operator==(const UrlRule& other) const;

  // Returns a protobuf representation of the rule.
  url_pattern_index::proto::UrlRule ToProtobuf() const;

  // Canonicalizes the rule, i.e. orders the |domains| list properly, determines
  // |url_pattern_type| based on |url_pattern| together with |anchor_*|'s and
  // modifies the |url_pattern| accordingly to reduce the amount of redundancy
  // in the pattern.
  void Canonicalize();

  // Canonicalizes the rule's |url_pattern|.
  void CanonicalizeUrlPattern();

  // This is an allowlist rule (aka rule-level exception).
  bool is_allowlist = false;
  // An anchor used at the beginning of the URL pattern.
  AnchorType anchor_left = url_pattern_index::proto::ANCHOR_TYPE_NONE;
  // The same for the end of the pattern. Never equals to ANCHOR_TYPE_SUBDOMAIN.
  AnchorType anchor_right = url_pattern_index::proto::ANCHOR_TYPE_NONE;
  // Restriction to first-party/third-party requests.
  TriState is_third_party = TriState::DONT_CARE;
  // Apply the filter only to addresses with matching case.
  // TODO(pkalinnikov): Implement case insensitivity in matcher.
  bool match_case = false;
  // A bitmask that reflects what ElementType's to block/allow and what
  // kinds of ActivationType's to associate this rule with.
  TypeMask type_mask = kDefaultElementTypes;
  // The list of domains to be included/excluded from the filter's affected set.
  // If a particular string in the list starts with '~' then the respective
  // domain is excluded, otherwise included. The list should be ordered by
  // |CanonicalizeDomainList|.
  std::vector<std::string> domains;
  // The type of the URL pattern's format.
  UrlPatternType url_pattern_type =
      url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING;
  // A URL pattern in either of UrlPatternType's formats, corresponding to
  // |url_pattern_type|.
  std::string url_pattern;
};

// A single CSS element hiding rule as parsed from EasyList.
struct StyleRule {
  // Constructs a default empty blocklist rule (no domains, empty selector).
  StyleRule();

  StyleRule(const StyleRule&);
  ~StyleRule();
  StyleRule& operator=(const StyleRule&);

  friend bool operator==(const StyleRule&, const StyleRule&) = default;

  // Returns a protobuf representation of the rule.
  url_pattern_index::proto::StyleRule ToProtobuf() const;

  // Canonicalizes the rule, i.e. orders the |domains| list properly.
  void Canonicalize();

  // This is an allowlist rule (aka rule-level exception).
  bool is_allowlist = false;
  // The list of domains, same as UrlRule::domains.
  std::vector<std::string> domains;
  // A CSS selector as specified in http://www.w3.org/TR/css3-selectors.
  std::string selector;
};

// Sorts domain patterns in decreasing order of length (and alphabetically
// within same-length groups).
void CanonicalizeDomainList(std::vector<std::string>* domains);

// Converts protobuf |rule| into its canonical EasyList string representation.
std::string ToString(const url_pattern_index::proto::UrlRule& rule);

// Analyzes `selector` and extracts anchors (classes and ids) if it is
// considered performant enough for the subresource filter. Returns true if the
// selector is supported, and populates `classes` and `ids`. A selector is
// considered not performant if it has no anchor and is either a global rule
// or a pseudo-class selector.
//
// The performance standard for style rules is as follows:
// 1. Anchored rules (classes or IDs, e.g., .ad, #id) are performant because
//    they can be indexed. Note that attribute selectors (e.g.,
//    [class*="ad-"]) are not used for anchor extraction and do not satisfy
//    the anchor requirement.
// 2. Site-specific rules without anchors (e.g., example.com##div,
//    example.com##[attr]) are allowed because their performance impact is
//    limited to a single domain.
// 3. Rules with pseudo-classes (e.g., :hover, :empty) always require a class
//    or ID anchor, even if site-specific, because they can be expensive to
//    evaluate and often act as pseudo-universal selectors.
// 4. Global unanchored rules (e.g., ##div, ##*, ##[attr]) are rejected as
//    they would be evaluated on every element of every page.
//
// IMPORTANT: Since this is not a perfect parser, it is feasible for classes
// and ids to be incorrectly parsed. Therefore, do not use the output of this
// method directly for matching (e.g., creating a stylesheet). It is intended
// to be used for direct indexing into the list of style rules in the indexed
// ruleset. This design can lead to either missing a style rule (false negative)
// which is safe, or adding the wrong global style rule into the document (safe,
// since global rules are meant to be on all documents anyway) so we don't get
// false positives.
bool GetAnchorsIfSupported(std::string_view selector,
                           bool is_site_specific,
                           std::vector<std::string>& classes,
                           std::vector<std::string>& ids);

std::string ToString(const url_pattern_index::proto::StyleRule& rule);

// For testing.
std::ostream& operator<<(std::ostream& os, const UrlRule& rule);
std::ostream& operator<<(std::ostream& os, const StyleRule& rule);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULE_PARSER_RULE_H_
