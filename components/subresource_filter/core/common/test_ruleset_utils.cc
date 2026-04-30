// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/core/common/test_ruleset_utils.h"

#include <string_view>
#include <utility>

namespace subresource_filter {
namespace testing {

namespace proto = url_pattern_index::proto;

namespace {

proto::UrlRule CreateRuleImpl(std::string_view substring,
                              bool is_allowlist_rule,
                              bool is_suffix_rule) {
  proto::UrlRule rule;

  rule.set_semantics(is_allowlist_rule ? proto::RULE_SEMANTICS_ALLOWLIST
                                       : proto::RULE_SEMANTICS_BLOCKLIST);
  rule.set_source_type(proto::SOURCE_TYPE_ANY);
  rule.set_element_types(proto::ELEMENT_TYPE_ALL);
  rule.set_url_pattern_type(proto::URL_PATTERN_TYPE_SUBSTRING);
  rule.set_anchor_left(proto::ANCHOR_TYPE_NONE);
  rule.set_anchor_right(is_suffix_rule ? proto::ANCHOR_TYPE_BOUNDARY
                                       : proto::ANCHOR_TYPE_NONE);
  rule.set_url_pattern(std::string(substring));

  return rule;
}

proto::UrlRule CreateRuleForDocumentImpl(
    std::string_view substring,
    int32_t activation_types,
    std::vector<std::string> initiator_domains,
    bool is_allowlist_rule,
    bool is_suffix_rule) {
  proto::UrlRule rule =
      CreateRuleImpl(substring, is_allowlist_rule, is_suffix_rule);
  rule.set_activation_types(activation_types);
  for (std::string& domain : initiator_domains) {
    rule.add_initiator_domains()->set_domain(std::move(domain));
  }
  return rule;
}

}  // namespace

proto::UrlRule CreateSubstringRule(std::string_view substring) {
  return CreateRuleImpl(substring, /*is_allowlist_rule=*/false,
                        /*is_suffix_rule=*/false);
}

proto::UrlRule CreateAllowlistSubstringRule(std::string_view substring) {
  return CreateRuleImpl(substring, /*is_allowlist_rule=*/true,
                        /*is_suffix_rule=*/false);
}

proto::UrlRule CreateSuffixRule(std::string_view suffix) {
  return CreateRuleImpl(suffix, /*is_allowlist_rule=*/false,
                        /*is_suffix_rule=*/true);
}

proto::UrlRule CreateAllowlistSuffixRule(std::string_view suffix) {
  return CreateRuleImpl(suffix, /*is_allowlist_rule=*/true,
                        /*is_suffix_rule=*/true);
}

proto::UrlRule CreateRuleForDocument(
    std::string_view pattern,
    int32_t activation_types,
    std::vector<std::string> initiator_domains) {
  return CreateRuleForDocumentImpl(pattern, activation_types, initiator_domains,
                                   /*is_allowlist_rule=*/false,
                                   /*is_suffix_rule=*/false);
}

proto::UrlRule CreateAllowlistRuleForDocument(
    std::string_view pattern,
    int32_t activation_types,
    std::vector<std::string> initiator_domains) {
  return CreateRuleForDocumentImpl(pattern, activation_types, initiator_domains,
                                   /*is_allowlist_rule=*/true,
                                   /*is_suffix_rule=*/false);
}

}  // namespace testing
}  // namespace subresource_filter
