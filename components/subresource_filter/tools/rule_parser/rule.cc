// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/rule_parser/rule.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_util.h"

namespace subresource_filter {

namespace {

// Returns the simplest matching strategy that can be used for the |rule|.
UrlPatternType GetUrlPatternType(const UrlRule& rule) {
  const std::string& url_pattern = rule.url_pattern;
  if (url_pattern.size() >= 2 && url_pattern.front() == '/' &&
      url_pattern.back() == '/') {
    return url_pattern_index::proto::URL_PATTERN_TYPE_REGEXP;
  }
  if (url_pattern.find('^') != std::string::npos ||
      rule.anchor_right != url_pattern_index::proto::ANCHOR_TYPE_NONE ||
      rule.anchor_left != url_pattern_index::proto::ANCHOR_TYPE_NONE) {
    return url_pattern_index::proto::URL_PATTERN_TYPE_WILDCARDED;
  }
  size_t wildcard_pos = url_pattern.find('*');

  if (wildcard_pos == std::string::npos ||
      wildcard_pos == url_pattern.size() - 1) {
    return url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING;
  }

  if (wildcard_pos == 0) {
    size_t next_wildcard = url_pattern.find('*', 1);
    if (next_wildcard == std::string::npos ||
        next_wildcard == url_pattern.size() - 1) {
      return url_pattern_index::proto::URL_PATTERN_TYPE_SUBSTRING;
    }
  }
  return url_pattern_index::proto::URL_PATTERN_TYPE_WILDCARDED;
}

void DomainListJoin(const google::protobuf::RepeatedPtrField<
                        url_pattern_index::proto::DomainListItem>& domain_list,
                    char separator,
                    std::string* output) {
  for (const url_pattern_index::proto::DomainListItem& item : domain_list) {
    if (item.exclude())
      *output += '~';
    *output += item.domain();
    *output += separator;
  }
  output->pop_back();  // Remove the last separator.
}

}  // namespace

UrlRule::UrlRule() = default;

UrlRule::UrlRule(const UrlRule&) = default;

UrlRule::~UrlRule() = default;

UrlRule& UrlRule::operator=(const UrlRule&) = default;

bool UrlRule::operator==(const UrlRule& other) const {
  return is_allowlist == other.is_allowlist &&
         is_third_party == other.is_third_party &&
         match_case == other.match_case && type_mask == other.type_mask &&
         domains == other.domains &&
         url_pattern_type == other.url_pattern_type &&
         url_pattern == other.url_pattern;
}

url_pattern_index::proto::UrlRule UrlRule::ToProtobuf() const {
  url_pattern_index::proto::UrlRule result;

  result.set_semantics(
      is_allowlist ? url_pattern_index::proto::RULE_SEMANTICS_ALLOWLIST
                   : url_pattern_index::proto::RULE_SEMANTICS_BLOCKLIST);
  switch (is_third_party) {
    case TriState::DONT_CARE:
      result.set_source_type(url_pattern_index::proto::SOURCE_TYPE_ANY);
      break;
    case TriState::YES:
      result.set_source_type(url_pattern_index::proto::SOURCE_TYPE_THIRD_PARTY);
      break;
    case TriState::NO:
      result.set_source_type(url_pattern_index::proto::SOURCE_TYPE_FIRST_PARTY);
      break;
    default:
      LOG(FATAL);
  }

  if (type_mask & kAllElementTypes)
    result.set_element_types(type_mask & kAllElementTypes);
  if (type_mask & kAllActivationTypes) {
    result.set_activation_types((type_mask & kAllActivationTypes) >>
                                kActivationTypesShift);
  }

  for (const std::string& domain : domains) {
    url_pattern_index::proto::DomainListItem* list_item =
        result.add_initiator_domains();
    if (domain.empty())
      continue;
    if (domain[0] == '~') {
      list_item->set_domain(domain.substr(1));
      list_item->set_exclude(true);
    } else {
      list_item->set_domain(domain);
    }
  }

  result.set_url_pattern_type(url_pattern_type);
  result.set_anchor_left(anchor_left);
  result.set_anchor_right(anchor_right);
  if (match_case != result.match_case())
    result.set_match_case(match_case);
  if (!url_pattern.empty())
    result.set_url_pattern(url_pattern);

  return result;
}

void UrlRule::Canonicalize() {
  url_pattern_type = GetUrlPatternType(*this);
  CanonicalizeUrlPattern();
  CanonicalizeDomainList(&domains);
}

CssRule::CssRule() = default;

CssRule::CssRule(const CssRule&) = default;

CssRule::~CssRule() = default;

CssRule& CssRule::operator=(const CssRule&) = default;

bool CssRule::operator==(const CssRule& other) const {
  return is_allowlist == other.is_allowlist && domains == other.domains &&
         css_selector == other.css_selector;
}

url_pattern_index::proto::CssRule CssRule::ToProtobuf() const {
  url_pattern_index::proto::CssRule result;

  result.set_semantics(
      is_allowlist ? url_pattern_index::proto::RULE_SEMANTICS_ALLOWLIST
                   : url_pattern_index::proto::RULE_SEMANTICS_BLOCKLIST);

  for (const std::string& domain : domains) {
    url_pattern_index::proto::DomainListItem* list_item = result.add_domains();
    if (domain.empty())
      continue;
    if (domain[0] == '~') {
      list_item->set_domain(domain.substr(1));
      list_item->set_exclude(true);
    } else {
      list_item->set_domain(domain);
    }
  }

  if (!css_selector.empty())
    result.set_css_selector(css_selector);

  return result;
}

void CssRule::Canonicalize() {
  CanonicalizeDomainList(&domains);
}

void UrlRule::CanonicalizeUrlPattern() {
  if (url_pattern_type == url_pattern_index::proto::URL_PATTERN_TYPE_REGEXP)
    return;
  // TODO(melandory): Canonicalize more, e.g. squeeze '/\*\*+/' sequences
  // down to one '*'.
  if (anchor_left != url_pattern_index::proto::ANCHOR_TYPE_SUBDOMAIN &&
      !url_pattern.empty() && url_pattern.front() == '*') {
    url_pattern.erase(0, 1);
    anchor_left = url_pattern_index::proto::ANCHOR_TYPE_NONE;
  }
  if (!url_pattern.empty() && url_pattern.back() == '*') {
    url_pattern.erase(url_pattern.size() - 1, 1);
    anchor_right = url_pattern_index::proto::ANCHOR_TYPE_NONE;
  }
}

void CanonicalizeDomainList(std::vector<std::string>* domains) {
  if (!domains->empty()) {
    std::sort(domains->begin(), domains->end(),
              [](const std::string& lhs, const std::string& rhs) {
                return lhs.size() > rhs.size() ||
                       (lhs.size() == rhs.size() && lhs < rhs);
              });
  }
}

std::string ToString(const url_pattern_index::proto::UrlRule& rule) {
  std::string result;

  switch (rule.semantics()) {
    case url_pattern_index::proto::RULE_SEMANTICS_BLOCKLIST:
      break;
    case url_pattern_index::proto::RULE_SEMANTICS_ALLOWLIST:
      result += "@@";
      break;
    default:
      LOG(FATAL);
  }

  switch (rule.anchor_left()) {
    case url_pattern_index::proto::ANCHOR_TYPE_NONE:
      break;
    case url_pattern_index::proto::ANCHOR_TYPE_BOUNDARY:
      result += '|';
      break;
    case url_pattern_index::proto::ANCHOR_TYPE_SUBDOMAIN:
      result += "||";
      break;
    default:
      LOG(FATAL);
  }
  result += rule.url_pattern();
  switch (rule.anchor_right()) {
    case url_pattern_index::proto::ANCHOR_TYPE_NONE:
      break;
    case url_pattern_index::proto::ANCHOR_TYPE_BOUNDARY:
      result += '|';
      break;
    default:
      LOG(FATAL);
  }

  std::vector<std::string> options;

  if (rule.element_types() != kDefaultElementTypes) {
    // Try to print as few element types as possible.
    int balance = 0;
    for (const auto& element_type : kElementTypes) {
      if (rule.element_types() & type_mask_for(element_type.type))
        ++balance;
      else
        --balance;
    }
    const bool print_positives = (balance <= 0);

    for (const auto& element_type : kElementTypes) {
      if (element_type.type == url_pattern_index::proto::ELEMENT_TYPE_POPUP)
        continue;

      const bool is_positive =
          rule.element_types() & type_mask_for(element_type.type);
      if (is_positive == print_positives) {
        if (is_positive)
          options.push_back(element_type.name);
        else
          options.push_back(std::string("~") + element_type.name);
      }
    }

    // The POPUP type is excluded by default, therefore should be printed last.
    if (rule.element_types() &
        type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_POPUP)) {
      const auto& popup_type = kElementTypes[11];
      CHECK_EQ(url_pattern_index::proto::ELEMENT_TYPE_POPUP, popup_type.type,
               base::NotFatalUntil::M129);
      options.push_back(popup_type.name);
    }
  }

  for (const auto& activation_type : kActivationTypes) {
    if (rule.activation_types() & activation_type.type)
      options.push_back(activation_type.name);
  }

  // This workaround produces a text rule which, when parsed back from text,
  // results in the same URL rule. Otherwise the parsed rule would have
  // element_types == kDefaultElementTypes instead of 0.
  if (!rule.element_types() && !rule.activation_types()) {
    const auto& image_type = kElementTypes[2];
    CHECK_EQ(url_pattern_index::proto::ELEMENT_TYPE_IMAGE, image_type.type,
             base::NotFatalUntil::M129);
    options.push_back(image_type.name);
    options.push_back(std::string("~") + image_type.name);
  }

  std::string source_type_string;
  switch (rule.source_type()) {
    case url_pattern_index::proto::SOURCE_TYPE_ANY:
      break;
    case url_pattern_index::proto::SOURCE_TYPE_FIRST_PARTY:
      source_type_string = "~";
      [[fallthrough]];
    case url_pattern_index::proto::SOURCE_TYPE_THIRD_PARTY:
      source_type_string += "third-party";
      options.push_back(std::move(source_type_string));
      break;
    default:
      LOG(FATAL);
  }

  if (rule.match_case())
    options.push_back("match-case");

  if (rule.initiator_domains_size()) {
    std::string domains = "domain=";
    DomainListJoin(rule.initiator_domains(), '|', &domains);
    options.push_back(std::move(domains));
  }

  if (!options.empty()) {
    result += '$';
    result += base::JoinString(options, ",");
  }

  return result;
}

std::string ToString(const url_pattern_index::proto::CssRule& rule) {
  std::string result;
  if (rule.domains_size())
    DomainListJoin(rule.domains(), ',', &result);

  switch (rule.semantics()) {
    case url_pattern_index::proto::RULE_SEMANTICS_BLOCKLIST:
      result += "##";
      break;
    case url_pattern_index::proto::RULE_SEMANTICS_ALLOWLIST:
      result += "#@#";
      break;
    default:
      LOG(FATAL);
  }

  return result += rule.css_selector();
}

std::ostream& operator<<(std::ostream& os, const UrlRule& rule) {
  return os << "UrlRule(" << ToString(rule.ToProtobuf()) << ")";
}

std::ostream& operator<<(std::ostream& os, const CssRule& rule) {
  return os << "CssRule(" << ToString(rule.ToProtobuf()) << ")";
}

}  // namespace subresource_filter
