// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/url_pattern_index/url_rule_util.h"

#include "base/macros.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"

namespace url_pattern_index {

namespace {

std::string AnchorToString(url_pattern_index::flat::AnchorType anchor_type) {
  switch (anchor_type) {
    case url_pattern_index::flat::AnchorType_NONE:
      return std::string();
    case url_pattern_index::flat::AnchorType_SUBDOMAIN:
      return "||";
    case url_pattern_index::flat::AnchorType_BOUNDARY:
      return "|";
  }
  NOTREACHED();
  return std::string();
}

// Class that aids in appending options to a pretty-printed rule.
class OptionsPrinter {
 public:
  OptionsPrinter() = default;

  // If this is the first printed option for the rule, add a $ separator,
  // otherwise a comma.
  std::string PrintOption(const std::string& option) {
    std::string out = printed_options_ ? "," : "$";
    printed_options_ = true;
    return out + option;
  }

 private:
  bool printed_options_ = false;

  DISALLOW_COPY_AND_ASSIGN(OptionsPrinter);
};

std::string PartyOptionsToString(
    OptionsPrinter* options_printer,
    const url_pattern_index::flat::UrlRule* flat_rule) {
  std::string out;
  bool third_party = flat_rule->options() &
                     url_pattern_index::flat::OptionFlag_APPLIES_TO_THIRD_PARTY;
  bool first_party = flat_rule->options() &
                     url_pattern_index::flat::OptionFlag_APPLIES_TO_FIRST_PARTY;
  if (first_party ^ third_party) {
    if (first_party)
      out += options_printer->PrintOption("~third-party");
    else
      out += options_printer->PrintOption("third-party");
  }
  return out;
}

std::string TypeOptionsToString(
    OptionsPrinter* options_printer,
    const url_pattern_index::flat::UrlRule* flat_rule) {
  std::string out;

  if (flat_rule->activation_types() &
      url_pattern_index::flat::ActivationType_DOCUMENT) {
    out += options_printer->PrintOption("document");
  }

  if (flat_rule->activation_types() &
      url_pattern_index::flat::ActivationType_GENERIC_BLOCK) {
    out += options_printer->PrintOption("genericblock");
  }

  // Filterlists do not support the "main_frame" and "csp_report" element types.
  // Hence we ignore them here.
  constexpr uint16_t kSupportedElementTypes =
      url_pattern_index::flat::ElementType_ANY &
      ~url_pattern_index::flat::ElementType_MAIN_FRAME &
      ~url_pattern_index::flat::ElementType_CSP_REPORT;
  uint16_t types = flat_rule->element_types() & kSupportedElementTypes;
  if (types == kSupportedElementTypes)
    return out;

  if (types & url_pattern_index::flat::ElementType_OTHER)
    out += options_printer->PrintOption("other");
  if (types & url_pattern_index::flat::ElementType_SCRIPT)
    out += options_printer->PrintOption("script");
  if (types & url_pattern_index::flat::ElementType_IMAGE)
    out += options_printer->PrintOption("image");
  if (types & url_pattern_index::flat::ElementType_STYLESHEET)
    out += options_printer->PrintOption("stylesheet");
  if (types & url_pattern_index::flat::ElementType_OBJECT)
    out += options_printer->PrintOption("object");
  if (types & url_pattern_index::flat::ElementType_XMLHTTPREQUEST)
    out += options_printer->PrintOption("xmlhttprequest");
  if (types & url_pattern_index::flat::ElementType_OBJECT_SUBREQUEST)
    out += options_printer->PrintOption("object-subrequest");
  if (types & url_pattern_index::flat::ElementType_SUBDOCUMENT)
    out += options_printer->PrintOption("subdocument");
  if (types & url_pattern_index::flat::ElementType_PING)
    out += options_printer->PrintOption("ping");
  if (types & url_pattern_index::flat::ElementType_MEDIA)
    out += options_printer->PrintOption("media");
  if (types & url_pattern_index::flat::ElementType_FONT)
    out += options_printer->PrintOption("font");
  if (types & url_pattern_index::flat::ElementType_WEBSOCKET)
    out += options_printer->PrintOption("websocket");

  return out;
}

std::string ConvertFlatString(const flatbuffers::String* string) {
  return string ? std::string(string->data(), string->size()) : "";
}

std::string DomainOptionsToString(
    OptionsPrinter* options_printer,
    const url_pattern_index::flat::UrlRule* flat_rule) {
  std::string out;
  if (!flat_rule->domains_included() && !flat_rule->domains_excluded())
    return "";

  out += options_printer->PrintOption("domain=");

  bool first = true;
  if (flat_rule->domains_included()) {
    for (auto* domain : *flat_rule->domains_included()) {
      if (!first)
        out += "|";
      first = false;
      out += ConvertFlatString(domain);
    }
  }
  if (flat_rule->domains_excluded()) {
    for (auto* domain : *flat_rule->domains_excluded()) {
      if (!first)
        out += "|";
      first = false;
      out += "~" + ConvertFlatString(domain);
    }
  }
  return out;
}

}  // namespace

std::string FlatUrlRuleToFilterlistString(const flat::UrlRule* flat_rule) {
  std::string out;

  if (flat_rule->options() & url_pattern_index::flat::OptionFlag_IS_WHITELIST)
    out += "@@";

  out += AnchorToString(flat_rule->anchor_left());

  std::string pattern = ConvertFlatString(flat_rule->url_pattern());

  // Add a wildcard to pattern if necessary to differentiate it from a regex.
  // E.g., /foo/ should be /foo/*.
  if (flat_rule->url_pattern_type() !=
          url_pattern_index::flat::UrlPatternType_REGEXP &&
      pattern.size() >= 2 && pattern[0] == '/' &&
      pattern[pattern.size() - 1] == '/') {
    pattern += "*";
  }
  out += pattern;

  out += AnchorToString(flat_rule->anchor_right());

  OptionsPrinter options_printer;

  out += PartyOptionsToString(&options_printer, flat_rule);

  // TODO(csharrison): Consider printing something for case-insensitive /
  // case-sensitive rules.

  out += TypeOptionsToString(&options_printer, flat_rule);

  out += DomainOptionsToString(&options_printer, flat_rule);

  return out;
}

}  // namespace url_pattern_index
