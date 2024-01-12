// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULE_PARSER_RULE_PARSER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULE_PARSER_RULE_PARSER_H_

#include <stddef.h>
#include <ostream>
#include <string>
#include <string_view>

#include "components/subresource_filter/tools/rule_parser/rule.h"

namespace subresource_filter {

// A parser of EasyList rules. It is intended to be (re-)used for parsing
// multiple rules.
// TODO(pkalinnikov): Support 'sitekey', 'collapse', and 'donottrack' options.
class RuleParser {
 public:
  // Detailed information about a parse error (if any).
  struct ParseError {
    // Indicates the type of an error occured during a Parse(...) call.
    enum ErrorCode {
      NONE,  // Parsing was successful.

      EMPTY_RULE,             // The parsed line does not contain any rule.
      BAD_ALLOWLIST_SYNTAX,   // Used wrong syntax for an allowlist rule.
      UNKNOWN_OPTION,         // Using of unknown option in a URL rule.
      NOT_A_TRISTATE_OPTION,  // Used negation for a non-tristate option.
      DEPRECATED_OPTION,      // Used a deprecated option.
      ALLOWLIST_ONLY_OPTION,  // The option applies to allowlist rules only.
      NO_VALUE_PROVIDED,      // A valued option is used without a value.

      WRONG_CSS_RULE_DELIM,  // Using of a wrong delimiter in a CSS rule.
      EMPTY_CSS_SELECTOR,    // No CSS selector specified in a CSS rule.

      UNSUPPORTED_FEATURE,  // Using not currently supported EasyList feature.
    };

    // TODO(pkalinnikov): Introduce warnings for, e.g., using an inverted
    // "document" activation type, using unsupported option, etc. This would let
    // a client have a best-effort version of the rule. Leave it up to clients
    // to decide what warnings/errors are critical for them.

    // Constructs a ParseError in a default (no error) state.
    ParseError();
    ~ParseError();

    ErrorCode error_code = NONE;

    // A copy of the parsed line. If no error occurred, it is empty.
    std::string line;

    // Position of the character in the |line| that introduced the error. If
    // |error_code| != NONE, then 0 <= |error_index| <= line.size(), otherwise
    // |error_index| == std::string::npos.
    size_t error_index = std::string::npos;
  };

  RuleParser();

  RuleParser(const RuleParser&) = delete;
  RuleParser& operator=(const RuleParser&) = delete;

  ~RuleParser();

  // Returns a human-readable detailed explanation of a parsing error.
  static const char* GetParseErrorCodeDescription(ParseError::ErrorCode code);

  // Parses a rule from the |line|. Returns the type of the rule parsed, or
  // RULE_TYPE_UNSPECIFIED on error. Notes:
  //  - When parsing a URL rule, URL syntax is not verified.
  //  - When parsing a CSS rule, the CSS selector syntax is not verified.
  RuleType Parse(std::string_view line);

  // Returns error diagnostics on the latest parsed line.
  const ParseError& parse_error() const { return parse_error_; }

  // Gets the last parsed rule type. It is guaranteed to return the same value
  // as the last Parse(...) invocation, or RULE_TYPE_UNSPECIFIED if no calls
  // were done.
  RuleType rule_type() const { return rule_type_; }

  // Gets the last parsed URL filtering rule. The result is undefined if
  // rule_type() != RULE_TYPE_URL,
  const UrlRule& url_rule() const { return url_rule_; }

  // Gets the last parsed CSS element hiding rule. The result is undefined if
  // rule_type() != RULE_TYPE_CSS.
  const CssRule& css_rule() const { return css_rule_; }

 private:
  // Parses the |part| and saves parsed URL filtering rule to the |url_rule_|
  // member. |origin| is used for a proper error reporting. Returns
  // RULE_TYPE_URL ff the |part| is a well-formed URL rule. Otherwise returns
  // RULE_TYPE_UNSPECIFIED and sets |parse_error_|.
  RuleType ParseUrlRule(std::string_view origin, std::string_view part);

  // Parses the |options| segment of a URL filtering rule and saves the parsed
  // options to the |url_rule_| member. Returns true if the options were parsed
  // correctly. Otherwise sets an error in |parse_error_| and returns false.
  bool ParseUrlRuleOptions(std::string_view origin, std::string_view options);

  // Parses the |part| and saves parsed CSS rule to the |css_rule_| member.
  // |css_section_start| denotes a position of '#' in the |part|, used to
  // separate a CSS selector. Returns true iff the line is a well-formed CSS
  // rule. Sets |parse_error_| on error.
  RuleType ParseCssRule(std::string_view origin,
                        std::string_view part,
                        size_t css_section_start);

  // Sets |parse_error_| to contain specific error, starting at |error_begin|.
  void SetParseError(ParseError::ErrorCode code,
                     std::string_view origin,
                     const char* error_begin);

  ParseError parse_error_;
  RuleType rule_type_;
  UrlRule url_rule_;
  CssRule css_rule_;
};

// Pretty-prints the parsing |error| to |out|, e.g. like this:
//   (error:22) Unknown URL rule option:
//   @@example.org$script,unknown_option
//                        ^
std::ostream& operator<<(std::ostream& out,
                         const RuleParser::ParseError& error);

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_TOOLS_RULE_PARSER_RULE_PARSER_H_
