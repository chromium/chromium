// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/tools/rule_parser/rule_parser.h"

#include <map>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/subresource_filter/tools/rule_parser/rule_options.h"
#include "components/url_pattern_index/proto/rules.pb.h"

namespace subresource_filter {

namespace {

// Encapsulates meta-information of URL rule options identified by keywords.
class KeywordMap {
 public:
  // Types of rule options that can be represented by keywords.
  enum OptionType {
    OPTION_UNDEFINED,
    OPTION_ELEMENT_TYPE,
    OPTION_ACTIVATION_TYPE,
    OPTION_THIRD_PARTY,
    OPTION_DOMAIN,
    OPTION_SITEKEY,
    OPTION_MATCH_CASE,
    OPTION_COLLAPSE,
    OPTION_DO_NOT_TRACK,
  };

  enum OptionFlag : int {
    FLAG_NONE = 0,
    // The option requires a value, e.g. 'domain=example.org'.
    FLAG_REQUIRES_VALUE = 1,
    // The option allows invertion, e.g. 'image' and '~image'.
    FLAG_IS_TRISTATE = 2,
    // The option can be used with allowlist rules only.
    FLAG_IS_ALLOWLIST_ONLY = 4,
    // The option is not supposed to be used any more.
    FLAG_IS_DEPRECATED = 8,
    // The option is not supported yet.
    FLAG_IS_NOT_SUPPORTED = 16,
  };

  // Meta-information about an option represented by a certain keyword.
  struct OptionDetails {
    // Creates an option that defines a filter for the specified |element_type|.
    // In addition to the provided |flags|, FLAG_IS_TRISTATE will always be set
    // by default.
    OptionDetails(url_pattern_index::proto::ElementType element_type, int flags)
        : type(OPTION_ELEMENT_TYPE),
          flags(FLAG_IS_TRISTATE | flags),
          element_type(element_type) {}

    // Creates an ActivationType option.
    explicit OptionDetails(
        url_pattern_index::proto::ActivationType activation_type)
        : type(OPTION_ACTIVATION_TYPE),
          flags(FLAG_IS_ALLOWLIST_ONLY),
          activation_type(activation_type) {}

    // Creates a generic option.
    OptionDetails(OptionType type, int flags) : type(type), flags(flags) {
      CHECK_NE(type, OPTION_ELEMENT_TYPE, base::NotFatalUntil::M129);
      CHECK_NE(type, OPTION_ACTIVATION_TYPE, base::NotFatalUntil::M129);
    }

    bool requires_value() const { return flags & FLAG_REQUIRES_VALUE; }
    bool is_tristate() const { return flags & FLAG_IS_TRISTATE; }
    bool is_ALLOWLIST_ONLY() const { return flags & FLAG_IS_ALLOWLIST_ONLY; }
    bool is_deprecated() const { return flags & FLAG_IS_DEPRECATED; }
    bool is_not_supported() const { return flags & FLAG_IS_NOT_SUPPORTED; }

    OptionType type = OPTION_UNDEFINED;

    // Stores various OptionFlag's combined using bitwise OR.
    int flags = FLAG_NONE;

    // The element type that this option defines a filter for, if any. Set to
    // ELEMENT_TYPE_UNSPECIFIED for non-ElementType options.
    url_pattern_index::proto::ElementType element_type =
        url_pattern_index::proto::ELEMENT_TYPE_UNSPECIFIED;

    // The activation type that this option includes to the rule. Set to
    // ACTIVATION_TYPE_UNSPECIFIED for non-ActivationType options.
    url_pattern_index::proto::ActivationType activation_type =
        url_pattern_index::proto::ACTIVATION_TYPE_UNSPECIFIED;
  };

  // Initializes the map with default keywords.
  KeywordMap();

  KeywordMap(const KeywordMap&) = delete;
  KeywordMap& operator=(const KeywordMap&) = delete;

  ~KeywordMap();

  // Returns detailed information associated with the provided |name| option.
  // Returns nullptr on unknown options.
  const OptionDetails* Lookup(std::string_view name) const;

 private:
  // Associates |details| with a specified option |name|.
  void AddOption(std::string_view name, const OptionDetails& details);

  std::map<std::string, OptionDetails> options_;
};

KeywordMap::KeywordMap() {
  // ElementType options.
  for (const auto& element_type : kElementTypes) {
    OptionDetails details(element_type.type, FLAG_NONE);
    AddOption(element_type.name, details);
  }
  // Deprecated ElementType options.
  for (const auto& element_type : kDeprecatedElementTypes) {
    OptionDetails details(element_type.maps_to_type, FLAG_IS_DEPRECATED);
    AddOption(element_type.name, details);
  }

  // ActivationType options.
  for (const auto& activation_type : kActivationTypes) {
    OptionDetails details(activation_type.type);
    AddOption(activation_type.name, details);
  }

  // TODO(pkalinnikov): Consider moving options metadata to a header.
  struct {
    const char* name;
    OptionType type;
    int flags;
  } const options[] = {
      // Tristate options.
      {"third-party", OPTION_THIRD_PARTY, FLAG_IS_TRISTATE},
      {"collapse", OPTION_COLLAPSE, FLAG_IS_TRISTATE | FLAG_IS_NOT_SUPPORTED},
      // Flag options.
      {"match-case", OPTION_MATCH_CASE, FLAG_NONE},
      {"donottrack", OPTION_DO_NOT_TRACK, FLAG_IS_NOT_SUPPORTED},
      // Value options.
      {"sitekey", OPTION_SITEKEY, FLAG_REQUIRES_VALUE | FLAG_IS_NOT_SUPPORTED},
      {"domain", OPTION_DOMAIN, FLAG_REQUIRES_VALUE},
  };

  for (const auto& option : options) {
    AddOption(option.name, OptionDetails(option.type, option.flags));
  }
}

KeywordMap::~KeywordMap() = default;

const KeywordMap::OptionDetails* KeywordMap::Lookup(
    std::string_view name) const {
  // TODO(pkalinnikov): Avoid std::string allocation.
  auto iterator = options_.find(std::string(name));
  return iterator != options_.end() ? &iterator->second : nullptr;
}

void KeywordMap::AddOption(std::string_view name,
                           const OptionDetails& details) {
  auto inserted = options_.insert(std::make_pair(std::string(name), details));
  CHECK(inserted.second, base::NotFatalUntil::M129);
}

KeywordMap* GetKeywordsMapSingleton() {
  // TODO(melandory): Get rid of this singleton.
  static auto* shared_keywords = new KeywordMap;
  return shared_keywords;
}

}  // namespace

// RuleParser ------------------------------------------------------------------

RuleParser::ParseError::ParseError() = default;
RuleParser::ParseError::~ParseError() = default;

RuleParser::RuleParser() = default;
RuleParser::~RuleParser() = default;

const char* RuleParser::GetParseErrorCodeDescription(
    ParseError::ErrorCode code) {
  switch (code) {
    case ParseError::NONE:
      return "Ok";
    case ParseError::EMPTY_RULE:
      return "The rule is empty";
    case ParseError::BAD_ALLOWLIST_SYNTAX:
      return "Wrong allowlist rule syntax";
    case ParseError::UNKNOWN_OPTION:
      return "Unknown URL rule option";
    case ParseError::NOT_A_TRISTATE_OPTION:
      return "Unexpected '~', the option is not invertable";
    case ParseError::DEPRECATED_OPTION:
      return "The option is deprecated";
    case ParseError::ALLOWLIST_ONLY_OPTION:
      return "The option can be used with allowlist rules only";
    case ParseError::NO_VALUE_PROVIDED:
      return "Expected '=', the option requires a value";
    case ParseError::WRONG_CSS_RULE_DELIM:
      return "Wrong CSS rule delimiter";
    case ParseError::EMPTY_CSS_SELECTOR:
      return "Expected non-empty CSS selector";
    case ParseError::UNSUPPORTED_FEATURE:
      return "The feature is not currently supported";
    default:
      return "Unknown error";
  }
}

// TODO(pkalinnikov): Refactor parsing approach to use a FSM.
RuleType RuleParser::Parse(std::string_view line) {
  rule_type_ = url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
  parse_error_ = ParseError();

  // Strip all leading and trailing whitespaces.
  std::string_view part = line;
  part = base::TrimWhitespaceASCII(part, base::TRIM_ALL);
  // Check whether it's a trivial rule.
  if (part.empty()) {
    // Note: cannot use part.data() here because it is flaky to rely on *which*
    // empty std::string_view StripWhitespace will return.
    SetParseError(ParseError::EMPTY_RULE, line, line.data());
    return url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
  }

  // Check whether it's a comment.
  // TODO(pkalinnikov): Handle special comments (e.g. 'Title', 'Expires' etc.).
  if (part[0] == '!' || part[0] == '[') {
    return rule_type_ = url_pattern_index::proto::RULE_TYPE_COMMENT;
  }

  // Suppose it is a CSS rule if a CSS-selector separator character ('#') is
  // present, followed by '#' or '@'.
  size_t css_separator_pos = part.find('#');
  for (; css_separator_pos != std::string_view::npos;
       css_separator_pos = part.find('#', css_separator_pos + 1)) {
    if (css_separator_pos + 1 == part.size()) {
      css_separator_pos = std::string_view::npos;
      break;
    }
    const char next_char = part[css_separator_pos + 1];
    if (next_char == '#' || next_char == '@')  // CSS rule starter.
      break;
  }

  if (css_separator_pos != std::string_view::npos) {
    return rule_type_ = ParseCssRule(line, part, css_separator_pos);
  }
  // Else assume we read a URL filtering rule.
  return rule_type_ = ParseUrlRule(line, part);
}

RuleType RuleParser::ParseUrlRule(std::string_view origin,
                                  std::string_view part) {
  CHECK(!part.empty() && part.data() >= origin.data());
  url_rule_ = UrlRule();

  // Check whether it's an allowlist rule.
  if (part[0] == '@') {
    part.remove_prefix(1);
    if (part.empty() || part[0] != '@') {
      SetParseError(ParseError::BAD_ALLOWLIST_SYNTAX, origin, part.data());
      return url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
    }
    part.remove_prefix(1);
    url_rule_.is_allowlist = true;
  }

  size_t options_start = part.rfind('$');
  // If the URL pattern is a regular expression, |options_start| might be
  // pointing to a character inside the pattern. This can happen for those rules
  // which don't have options at all, e.g., "/.*substring$/". All such rules end
  // with '/', therefore the following code can detect them to work around.
  if (options_start != std::string_view::npos && part.back() == '/') {
    options_start = std::string_view::npos;
  }

  if (options_start != std::string_view::npos) {
    const std::string_view options = part.substr(options_start + 1);
    if (!ParseUrlRuleOptions(origin, options))
      return url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
    part.remove_suffix(part.size() - options_start);
  }

  // Check for a left anchor.
  if (!part.empty() && part[0] == '|') {
    part.remove_prefix(1);
    if (!part.empty() && part[0] == '|') {
      part.remove_prefix(1);
      url_rule_.anchor_left = url_pattern_index::proto::ANCHOR_TYPE_SUBDOMAIN;
    } else {
      url_rule_.anchor_left = url_pattern_index::proto::ANCHOR_TYPE_BOUNDARY;
    }
  }

  // Check for a right anchor.
  if (!part.empty()) {
    if (part[part.size() - 1] == '|') {
      part.remove_suffix(1);
      url_rule_.anchor_right = url_pattern_index::proto::ANCHOR_TYPE_BOUNDARY;
    }
  }

  url_rule_.url_pattern = std::string(part);
  url_rule_.Canonicalize();

  return url_pattern_index::proto::RULE_TYPE_URL;
}

bool RuleParser::ParseUrlRuleOptions(std::string_view origin,
                                     std::string_view options) {
  CHECK_GE(options.data(), origin.data());

  bool has_seen_element_or_activation_type = false;
  for (std::string_view piece : base::SplitStringPiece(
           options, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    CHECK(!piece.empty(), base::NotFatalUntil::M129);

    TriState tri_state = TriState::YES;
    if (base::StartsWith(piece, "~", base::CompareCase::SENSITIVE)) {
      piece.remove_prefix(1);
      tri_state = TriState::NO;
    }

    size_t option_name_end = piece.find('=');
    std::string_view option_name = piece.substr(0, option_name_end);

    const auto* option_details = GetKeywordsMapSingleton()->Lookup(option_name);
    if (!option_details) {
      // TODO(pkalinnikov): Add a flag to RuleParser allowing unknown options.
      SetParseError(ParseError::UNKNOWN_OPTION, origin, option_name.data());
      return false;
    }

    if (tri_state == TriState::NO && !option_details->is_tristate()) {
      SetParseError(ParseError::NOT_A_TRISTATE_OPTION, origin,
                    option_name.data());
      return false;
    }

    if (option_details->requires_value() &&
        option_name_end == std::string_view::npos) {
      SetParseError(ParseError::NO_VALUE_PROVIDED, origin, option_name.data());
      return false;
    }

    if (option_details->is_ALLOWLIST_ONLY() && !url_rule_.is_allowlist) {
      SetParseError(ParseError::ALLOWLIST_ONLY_OPTION, origin,
                    option_name.data());
      return false;
    }

    if (option_details->is_deprecated()) {
      // TODO(pkalinnikov): Add a flag to RuleParser allowing deprecated
      // options (and issuing kind of a warning).
      SetParseError(ParseError::DEPRECATED_OPTION, origin, option_name.data());
      return false;
    }

    if (option_details->is_not_supported()) {
      // TODO(pkalinnikov): Add a flag to RuleParser allowing unsupported
      // features.
      SetParseError(ParseError::UNSUPPORTED_FEATURE, origin,
                    option_name.data());
      return false;
    }

    switch (option_details->type) {
      case KeywordMap::OPTION_ELEMENT_TYPE: {
        // The sign of the first element type option encountered determines
        // whether the unspecified element types will be included (if the first
        // option is negated) or excluded (otherwise).
        if (tri_state == TriState::YES) {
          // TODO(pkalinnikov): How about not resetting ActivationType options?
          if (!has_seen_element_or_activation_type)
            url_rule_.type_mask = 0;
          url_rule_.type_mask |= type_mask_for(option_details->element_type);
        } else {
          CHECK(tri_state == TriState::NO, base::NotFatalUntil::M129);
          url_rule_.type_mask &= ~type_mask_for(option_details->element_type);
        }
        has_seen_element_or_activation_type = true;
        break;
      }
      case KeywordMap::OPTION_ACTIVATION_TYPE:
        if (!has_seen_element_or_activation_type)
          url_rule_.type_mask = 0;
        url_rule_.type_mask |= type_mask_for(option_details->activation_type);
        has_seen_element_or_activation_type = true;
        break;
      case KeywordMap::OPTION_THIRD_PARTY:
        url_rule_.is_third_party = tri_state;
        break;
      case KeywordMap::OPTION_MATCH_CASE:
        url_rule_.match_case = true;
        break;
      case KeywordMap::OPTION_DOMAIN:
        url_rule_.domains =
            base::SplitString(piece.substr(option_name_end + 1), "|",
                              base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
        break;
      default:
        LOG(FATAL);
    }
  }

  return true;
}

RuleType RuleParser::ParseCssRule(std::string_view origin,
                                  std::string_view part,
                                  size_t css_section_start) {
  CHECK(part.data() >= origin.data());
  css_rule_ = CssRule();

  // Check for a list of domains.
  if (css_section_start) {
    CHECK(css_section_start != std::string_view::npos,
          base::NotFatalUntil::M129);
    auto pieces = base::SplitStringPiece(part.substr(0, css_section_start), ",",
                                         base::TRIM_WHITESPACE,
                                         base::SPLIT_WANT_NONEMPTY);
    for (std::string_view domain : pieces) {
      CHECK(!domain.empty(), base::NotFatalUntil::M129);
      css_rule_.domains.push_back(std::string(domain));
    }
  }

  part.remove_prefix(css_section_start + 1);
  if (part.empty()) {
    SetParseError(ParseError::WRONG_CSS_RULE_DELIM, origin, part.data());
    return url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
  }
  if (part[0] == '@') {
    css_rule_.is_allowlist = true;
    part.remove_prefix(1);
  }
  if (part.empty() || part[0] != '#') {
    SetParseError(ParseError::WRONG_CSS_RULE_DELIM, origin, part.data());
    return url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
  }
  part.remove_prefix(1);

  if (part.empty()) {
    SetParseError(ParseError::EMPTY_CSS_SELECTOR, origin, part.data());
    return url_pattern_index::proto::RULE_TYPE_UNSPECIFIED;
  }

  css_rule_.css_selector = std::string(part);
  css_rule_.Canonicalize();
  return url_pattern_index::proto::RULE_TYPE_CSS;
}

void RuleParser::SetParseError(ParseError::ErrorCode code,
                               std::string_view origin,
                               const char* error_begin) {
  CHECK(code != ParseError::NONE, base::NotFatalUntil::M129);
  CHECK(error_begin >= origin.data(), base::NotFatalUntil::M129);

  parse_error_.error_code = code;
  parse_error_.line = std::string(origin);
  parse_error_.error_index = error_begin - origin.data();
}

std::ostream& operator<<(std::ostream& out,
                         const RuleParser::ParseError& error) {
  if (error.error_code != RuleParser::ParseError::NONE) {
    out << "(error:" << error.error_index + 1 << ") "
        << RuleParser::GetParseErrorCodeDescription(error.error_code) << ":\n"
        << error.line << '\n'
        << std::string(error.error_index, ' ') << "^\n";
  }
  return out;
}

}  // namespace subresource_filter
