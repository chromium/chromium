// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/rule_parser/rule.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
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
    if (item.exclude()) {
      *output += '~';
    }
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

  if (type_mask & kAllElementTypes) {
    result.set_element_types(type_mask & kAllElementTypes);
  }
  if (type_mask & kAllActivationTypes) {
    result.set_activation_types((type_mask & kAllActivationTypes) >>
                                kActivationTypesShift);
  }

  for (const std::string& domain : domains) {
    url_pattern_index::proto::DomainListItem* list_item =
        result.add_initiator_domains();
    if (domain.empty()) {
      continue;
    }
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
  if (match_case != result.match_case()) {
    result.set_match_case(match_case);
  }
  if (!url_pattern.empty()) {
    result.set_url_pattern(url_pattern);
  }

  return result;
}

void UrlRule::Canonicalize() {
  url_pattern_type = GetUrlPatternType(*this);
  CanonicalizeUrlPattern();
  CanonicalizeDomainList(&domains);
}

StyleRule::StyleRule() = default;

StyleRule::StyleRule(const StyleRule&) = default;

StyleRule::~StyleRule() = default;

StyleRule& StyleRule::operator=(const StyleRule&) = default;

url_pattern_index::proto::StyleRule StyleRule::ToProtobuf() const {
  url_pattern_index::proto::StyleRule result;

  result.set_semantics(
      is_allowlist ? url_pattern_index::proto::RULE_SEMANTICS_ALLOWLIST
                   : url_pattern_index::proto::RULE_SEMANTICS_BLOCKLIST);

  for (const std::string& domain : domains) {
    url_pattern_index::proto::DomainListItem* list_item = result.add_domains();
    if (domain.empty()) {
      continue;
    }
    if (domain[0] == '~') {
      list_item->set_domain(domain.substr(1));
      list_item->set_exclude(true);
    } else {
      list_item->set_domain(domain);
    }
  }

  if (!selector.empty()) {
    result.set_selector(selector);
  }

  return result;
}

void StyleRule::Canonicalize() {
  CanonicalizeDomainList(&domains);
}

void UrlRule::CanonicalizeUrlPattern() {
  if (url_pattern_type == url_pattern_index::proto::URL_PATTERN_TYPE_REGEXP) {
    return;
  }
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
      if (rule.element_types() & type_mask_for(element_type.type)) {
        ++balance;
      } else {
        --balance;
      }
    }
    const bool print_positives = (balance <= 0);

    for (const auto& element_type : kElementTypes) {
      if (element_type.type == url_pattern_index::proto::ELEMENT_TYPE_POPUP) {
        continue;
      }

      const bool is_positive =
          rule.element_types() & type_mask_for(element_type.type);
      if (is_positive == print_positives) {
        if (is_positive) {
          options.push_back(element_type.name);
        } else {
          options.push_back(std::string("~") + element_type.name);
        }
      }
    }

    // The POPUP type is excluded by default, therefore should be printed last.
    if (rule.element_types() &
        type_mask_for(url_pattern_index::proto::ELEMENT_TYPE_POPUP)) {
      const auto& popup_type = kElementTypes[11];
      CHECK_EQ(url_pattern_index::proto::ELEMENT_TYPE_POPUP, popup_type.type);
      options.push_back(popup_type.name);
    }
  }

  for (const auto& activation_type : kActivationTypes) {
    if (rule.activation_types() & activation_type.type) {
      options.push_back(activation_type.name);
    }
  }

  // This workaround produces a text rule which, when parsed back from text,
  // results in the same URL rule. Otherwise the parsed rule would have
  // element_types == kDefaultElementTypes instead of 0.
  if (!rule.element_types() && !rule.activation_types()) {
    const auto& image_type = kElementTypes[2];
    CHECK_EQ(url_pattern_index::proto::ELEMENT_TYPE_IMAGE, image_type.type);
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

  if (rule.match_case()) {
    options.push_back("match-case");
  }

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

std::string ToString(const url_pattern_index::proto::StyleRule& rule) {
  std::string result;
  if (rule.domains_size()) {
    DomainListJoin(rule.domains(), ',', &result);
  }

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

  return result += rule.selector();
}

std::ostream& operator<<(std::ostream& os, const UrlRule& rule) {
  return os << "UrlRule(" << ToString(rule.ToProtobuf()) << ")";
}

std::ostream& operator<<(std::ostream& os, const StyleRule& rule) {
  return os << "StyleRule(" << ToString(rule.ToProtobuf()) << ")";
}

bool GetAnchorsIfSupported(std::string_view selector,
                           bool is_site_specific,
                           std::vector<std::string>& classes,
                           std::vector<std::string>& ids) {
  // Skip empty selectors and at-rules (e.g., @media).
  if (selector.empty() || selector.starts_with('@')) {
    return false;
  }

  // State variables for the parser.
  bool has_pseudo = false;
  bool in_attribute = false;
  bool in_quotes = false;
  char quote_char = 0;

  std::string current_name;
  char identifier_type = 0;  // '.' for class, '#' for ID, 0 for none.

  bool in_escape = false;

  for (char c : selector) {
    // CSS identifiers allow a broad range of characters, especially if escaped.
    // We treat anything non-ASCII, alphanumeric, '-', '_', or escaped as an
    // identifier character.
    bool is_id_char = (c & 0x80) || (c >= 'a' && c <= 'z') ||
                      (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '-' || c == '_' || in_escape;

    // If we are currently parsing an identifier and hit a delimiter (like a
    // space, dot, or hash) that is NOT part of an escape sequence, we finish
    // the identifier.
    if (identifier_type && !is_id_char && c != '\\') {
      if (!current_name.empty()) {
        if (identifier_type == '.') {
          classes.emplace_back(std::move(current_name));
        } else {
          ids.emplace_back(std::move(current_name));
        }
        current_name.clear();
      }
      identifier_type = 0;
    }

    // Handle escapes. In CSS, a backslash escapes the next character.
    // We skip the backslash itself and treat the next character as part of the
    // identifier.
    if (c == '\\' && !in_escape) {
      in_escape = true;
      continue;  // Skip the escape character itself
    }

    // Handle quotes. Characters inside quotes are ignored for anchor
    // extraction.
    if ((c == '"' || c == '\'') && !in_escape) {
      if (!in_quotes) {
        in_quotes = true;
        quote_char = c;
      } else if (c == quote_char) {
        in_quotes = false;
      }
    }

    // Handle attribute selectors. Characters inside [...] are ignored.
    else if (!in_quotes && c == '[' && !in_escape) {
      in_attribute = true;
    } else if (!in_quotes && c == ']' && !in_escape) {
      in_attribute = false;
    }

    // Outside of quotes and attributes, look for anchors and pseudo-classes.
    else if (!in_attribute && !in_escape) {
      if (c == '.' || c == '#') {
        identifier_type = c;
      } else if (c == ':') {
        has_pseudo = true;
      }
    }
    // Append character to the current identifier if we are inside one.
    if (identifier_type && (is_id_char || in_escape)) {
      current_name += c;
    }

    in_escape = false;
  }

  // Handle the last identifier if the selector ends with one.
  if (identifier_type && !current_name.empty()) {
    if (identifier_type == '.') {
      classes.emplace_back(std::move(current_name));
    } else {
      ids.emplace_back(std::move(current_name));
    }
  }

  if (has_pseudo || !is_site_specific) {
    return !classes.empty() || !ids.empty();
  }

  // Site-specific rules without pseudo-classes are allowed even without anchors
  // (e.g., tag selectors or attribute selectors specific to a site).
  return true;
}

}  // namespace subresource_filter
