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
  bool operator!=(const UrlRule& other) const { return !operator==(other); }

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
struct CssRule {
  // Constructs a default empty blocklist rule (no domains, empty selector).
  CssRule();

  CssRule(const CssRule&);
  ~CssRule();
  CssRule& operator=(const CssRule&);

  bool operator==(const CssRule& other) const;
  bool operator!=(const CssRule& other) const { return !operator==(other); }

  // Returns a protobuf representation of the rule.
  url_pattern_index::proto::CssRule ToProtobuf() const;

  // Canonicalizes the rule, i.e. orders the |domains| list properly.
  void Canonicalize();

  // This is an allowlist rule (aka rule-level exception).
  bool is_allowlist = false;
  // The list of domains, same as UrlRule::domains.
  std::vector<std::string> domains;
  // A CSS selector as specified in http://www.w3.org/TR/css3-selectors.
  std::string css_selector;
};

// Sorts domain patterns in decreasing order of length (and alphabetically
// within same-length groups).
void CanonicalizeDomainList(std::vector<std::string>* domains);

// Converts protobuf |rule| into its canonical EasyList string representation.
std::string ToString(const url_pattern_index::proto::UrlRule& rule);
std::string ToString(const url_pattern_index::proto::CssRule& rule);

// For testing.
std::ostream& operator<<(std::ostream& os, const UrlRule& rule);
std::ostream& operator<<(std::ostream& os, const CssRule& rule);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULE_PARSER_RULE_H_
