// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/web/js_filter_builder.h"

#include <memory>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/devtools/devtools/domains/types_runtime.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data_util.h"

namespace autofill_assistant {

JsFilterBuilder::JsFilterBuilder() = default;
JsFilterBuilder::~JsFilterBuilder() = default;

std::unique_ptr<base::Value> JsFilterBuilder::BuildArgumentArray() const {
  auto str_array_arg = std::make_unique<base::Value>(base::Value::Type::LIST);
  for (const std::string& str : arguments_) {
    str_array_arg->Append(str);
  }
  return str_array_arg;
}

std::vector<std::unique_ptr<runtime::CallArgument>>
JsFilterBuilder::BuildArgumentList() const {
  std::vector<std::unique_ptr<runtime::CallArgument>> arguments;
  arguments.emplace_back(
      runtime::CallArgument::Builder().SetValue(BuildArgumentArray()).Build());
  return arguments;
}

// clang-format off
std::string JsFilterBuilder::BuildFunction() const {
  return base::StrCat({
    R"(
    function(args) {
      let elements = [this];
    )",
    snippet_.ToString(),
    R"(
      if (elements.length == 0) return null;
      if (elements.length == 1) { return elements[0] }
      return elements;
    })"
  });
}
// clang-format on

bool JsFilterBuilder::AddFilter(const SelectorProto::Filter& filter) {
  switch (filter.filter_case()) {
    case SelectorProto::Filter::kCssSelector:
      // We querySelectorAll the current elements and remove duplicates, which
      // are likely when using inner text before CSS selector filters. We must
      // not return duplicates as they cause incorrect TOO_MANY_ELEMENTS errors.
      DefineQueryAllDeduplicated();
      AddLine({"elements = queryAllDeduplicated(elements, ",
               AddArgument(filter.css_selector()), ");"});
      return true;

    case SelectorProto::Filter::kInnerText:
      AddRegexpFilter(filter.inner_text(), "innerText");
      return true;

    case SelectorProto::Filter::kValue:
      AddRegexpFilter(filter.value(), "value");
      return true;

    case SelectorProto::Filter::kProperty:
      AddRegexpFilter(filter.property().text_filter(),
                      filter.property().property());
      return true;

    case SelectorProto::Filter::kBoundingBox:
      if (filter.bounding_box().require_nonempty()) {
        AddLine("elements = elements.filter((e) => {");
        AddLine("  const rect = e.getBoundingClientRect();");
        AddLine("  return rect.width != 0 && rect.height != 0;");
        AddLine("});");
      } else {
        AddLine(
            "elements = elements.filter((e) => e.getClientRects().length > "
            "0);");
      }
      return true;

    case SelectorProto::Filter::kPseudoElementContent: {
      // When a content is set, window.getComputedStyle().content contains a
      // double-quoted string with the content, unquoted here by JSON.parse().
      std::string re_var =
          AddRegexpInstance(filter.pseudo_element_content().content());
      std::string pseudo_type =
          PseudoTypeName(filter.pseudo_element_content().pseudo_type());

      AddLine("elements = elements.filter((e) => {");
      AddLine({"  const s = window.getComputedStyle(e, '", pseudo_type, "');"});
      AddLine("  if (!s || !s.content || !s.content.startsWith('\"')) {");
      AddLine("    return false;");
      AddLine("  }");
      AddLine({"  return ", re_var, ".test(JSON.parse(s.content));"});
      AddLine("});");
      return true;
    }

    case SelectorProto::Filter::kCssStyle: {
      std::string re_var = AddRegexpInstance(filter.css_style().value());
      std::string property = AddArgument(filter.css_style().property());
      std::string element = AddArgument(filter.css_style().pseudo_element());
      AddLine("elements = elements.filter((e) => {");
      AddLine("  const s = window.getComputedStyle(e, ");
      AddLine({"      ", element, " === '' ? null : ", element, ");"});
      AddLine({"  const match = ", re_var, ".test(s[", property, "]);"});
      if (filter.css_style().should_match()) {
        AddLine("  return match;");
      } else {
        AddLine("  return !match;");
      }
      AddLine("});");
      return true;
    }

    case SelectorProto::Filter::kLabelled:
      AddLine("elements = elements.flatMap((e) => {");
      AddLine(
          "  return e.tagName === 'LABEL' && e.control ? [e.control] : [];");
      AddLine("});");
      return true;

    case SelectorProto::Filter::kMatchCssSelector:
      AddLine({"elements = elements.filter((e) => e.webkitMatchesSelector(",
               AddArgument(filter.match_css_selector()), "));"});
      return true;

    case SelectorProto::Filter::kOnTop:
      AddLine("elements = elements.filter((e) => {");
      AddLine("if (e.getClientRects().length == 0) return false;");
      if (filter.on_top().scroll_into_view_if_needed()) {
        AddLine("e.scrollIntoViewIfNeeded(false);");
      }
      AddReturnIfOnTop(
          &snippet_, "e", /* on_top= */ "true", /* not_on_top= */ "false",
          /* not_in_view= */ filter.on_top().accept_element_if_not_in_view()
              ? "true"
              : "false");
      AddLine("});");
      return true;

    case SelectorProto::Filter::kNthMatch: {
      std::string index = base::NumberToString(filter.nth_match().index());
      AddLine({"elements = ", index, " < elements.length ? [elements[", index,
               "]] : [];"});
      return true;
    }

    case SelectorProto::Filter::kEnterFrame:
    case SelectorProto::Filter::kPseudoType:
    case SelectorProto::Filter::FILTER_NOT_SET:
      return false;
  }
}

void JsFilterBuilder::ClearResultsIfMoreThanOneResult() {
  AddLine("if (elements.length > 1) return [];");
}

std::string JsFilterBuilder::AddRegexpInstance(const TextFilter& filter) {
  std::string re_flags = filter.case_sensitive() ? "" : "i";
  std::string re_var = DeclareVariable();
  AddLine({"const ", re_var, " = RegExp(", AddArgument(filter.re2()), ", '",
           re_flags, "');"});
  return re_var;
}

void JsFilterBuilder::AddRegexpFilter(const TextFilter& filter,
                                      const std::string& property) {
  std::string re_var = AddRegexpInstance(filter);
  AddLine({"elements = elements.filter((e) => ", re_var, ".test(e.", property,
           "));"});
}

std::string JsFilterBuilder::DeclareVariable() {
  return base::StrCat({"v", base::NumberToString(variable_counter_++)});
}

std::string JsFilterBuilder::AddArgument(const std::string& value) {
  int index = arguments_.size();
  arguments_.emplace_back(value);
  return base::StrCat({"args[", base::NumberToString(index), "]"});
}

void JsFilterBuilder::DefineQueryAllDeduplicated() {
  // Ensure that we don't define the function more than once.
  if (defined_query_all_deduplicated_)
    return;

  defined_query_all_deduplicated_ = true;

  AddLine(R"(
    const queryAllDeduplicated = function(roots, selector) {
      if (roots.length == 0) {
        return [];
      }

      const matchesSet = new Set();
      const matches = [];
      roots.forEach((root) => {
        root.querySelectorAll(selector).forEach((elem) => {
          if (!matchesSet.has(elem)) {
            matchesSet.add(elem);
            matches.push(elem);
          }
        });
      });
      return matches;
    }
  )");
}
}  // namespace autofill_assistant
