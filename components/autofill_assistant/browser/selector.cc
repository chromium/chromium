// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/selector.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "components/autofill_assistant/browser/action_value.pb.h"
#include "components/autofill_assistant/browser/field_formatter.h"

namespace autofill_assistant {

// Comparison operations are in the autofill_assistant scope, even though
// they're not shared outside of this module, for them to be visible to
// std::make_tuple and std::lexicographical_compare.

bool operator<(const TextFilter& a, const TextFilter& b) {
  return std::make_tuple(a.re2(), a.case_sensitive()) <
         std::make_tuple(b.re2(), b.case_sensitive());
}

bool operator<(const AutofillValueRegexp& a, const AutofillValueRegexp& b) {
  return std::make_tuple(a.profile().identifier(),
                         field_formatter::GetHumanReadableValueExpression(
                             a.value_expression_re2().value_expression()),
                         a.value_expression_re2().case_sensitive()) <
         std::make_tuple(b.profile().identifier(),
                         field_formatter::GetHumanReadableValueExpression(
                             b.value_expression_re2().value_expression()),
                         b.value_expression_re2().case_sensitive());
}

bool operator<(const SelectorProto::PropertyFilter& a,
               const SelectorProto::PropertyFilter& b) {
  if (a.property() < b.property()) {
    return true;
  }
  if (a.property() != b.property()) {
    return false;
  }
  if (a.value_case() < b.value_case()) {
    return true;
  }
  if (a.value_case() != b.value_case()) {
    return false;
  }
  switch (a.value_case()) {
    case SelectorProto::PropertyFilter::kTextFilter:
      return a.text_filter() < b.text_filter();
    case SelectorProto::PropertyFilter::kAutofillValueRegexp:
      return a.autofill_value_regexp() < b.autofill_value_regexp();
    case SelectorProto::PropertyFilter::VALUE_NOT_SET:
      return false;
  }
}

bool operator<(const SelectorProto::SemanticFilter& a,
               const SelectorProto::SemanticFilter& b) {
  return std::make_tuple(a.objective(), a.role(), a.ignore_objective()) <
         std::make_tuple(b.objective(), b.role(), b.ignore_objective());
}

// Used by operator<(RepeatedPtrField<Filter>, RepeatedPtrField<Filter>)
bool operator<(const SelectorProto::Filter& a, const SelectorProto::Filter& b);

bool operator<(
    const google::protobuf::RepeatedPtrField<SelectorProto::Filter>& a,
    const google::protobuf::RepeatedPtrField<SelectorProto::Filter>& b) {
  return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
}

bool operator<(const SelectorProto::Filter& a, const SelectorProto::Filter& b) {
  if (a.filter_case() < b.filter_case()) {
    return true;
  }
  if (a.filter_case() != b.filter_case()) {
    return false;
  }
  switch (a.filter_case()) {
    case SelectorProto::Filter::kCssSelector:
      return a.css_selector() < b.css_selector();

    case SelectorProto::Filter::kInnerText:
      return a.inner_text() < b.inner_text();

    case SelectorProto::Filter::kValue:
      return a.value() < b.value();

    case SelectorProto::Filter::kPseudoType:
      return a.pseudo_type() < b.pseudo_type();

    case SelectorProto::Filter::kPseudoElementContent:
      return std::make_tuple(a.pseudo_element_content().pseudo_type(),
                             a.pseudo_element_content().content()) <
             std::make_tuple(b.pseudo_element_content().pseudo_type(),
                             b.pseudo_element_content().content());

    case SelectorProto::Filter::kCssStyle:
      return std::make_tuple(a.css_style().property(),
                             a.css_style().pseudo_element(),
                             a.css_style().value()) <
             std::make_tuple(b.css_style().property(),
                             b.css_style().pseudo_element(),
                             b.css_style().value());

    case SelectorProto::Filter::kOnTop:
      return std::make_tuple(a.on_top().scroll_into_view_if_needed(),
                             a.on_top().accept_element_if_not_in_view()) <
             std::make_tuple(b.on_top().scroll_into_view_if_needed(),
                             b.on_top().accept_element_if_not_in_view());

    case SelectorProto::Filter::kBoundingBox:
      return a.bounding_box().require_nonempty() <
             b.bounding_box().require_nonempty();

    case SelectorProto::Filter::kEnterFrame:
    case SelectorProto::Filter::kLabelled:
    case SelectorProto::Filter::kParent:
      return false;

    case SelectorProto::Filter::kMatchCssSelector:
      return a.match_css_selector() < b.match_css_selector();

    case SelectorProto::Filter::kNthMatch:
      return a.nth_match().index() < b.nth_match().index();

    case SelectorProto::Filter::kProperty:
      return a.property() < b.property();

    case SelectorProto::Filter::kSemantic:
      return a.semantic() < b.semantic();

    case SelectorProto::Filter::FILTER_NOT_SET:
      return false;
  }
}

SelectorProto ToSelectorProto(const std::string& s) {
  return ToSelectorProto(std::vector<std::string>{s});
}

SelectorProto ToSelectorProto(const std::vector<std::string>& s) {
  SelectorProto proto;
  if (!s.empty()) {
    for (size_t i = 0; i < s.size(); i++) {
      if (i > 0) {
        proto.add_filters()->mutable_nth_match()->set_index(0);
        proto.add_filters()->mutable_enter_frame();
      }
      proto.add_filters()->set_css_selector(s[i]);
    }
  }
  return proto;
}

std::string PseudoTypeName(PseudoType pseudo_type) {
  switch (pseudo_type) {
    case UNDEFINED:
      return "undefined";
    case FIRST_LINE:
      return "first-line";
    case FIRST_LETTER:
      return "first-letter";
    case BEFORE:
      return "before";
    case AFTER:
      return "after";
    case BACKDROP:
      return "backdrop";
    case SELECTION:
      return "selection";
    case FIRST_LINE_INHERITED:
      return "first-line-inherited";
    case SCROLLBAR:
      return "scrollbar";
    case SCROLLBAR_THUMB:
      return "scrollbar-thumb";
    case SCROLLBAR_BUTTON:
      return "scrollbar-button";
    case SCROLLBAR_TRACK:
      return "scrollbar-track";
    case SCROLLBAR_TRACK_PIECE:
      return "scrollbar-track-piece";
    case SCROLLBAR_CORNER:
      return "scrollbar-corner";
    case RESIZER:
      return "resizer";
    case INPUT_LIST_BUTTON:
      return "input-list-button";

      // Intentionally no default case to make compilation fail if a new value
      // was added to the enum but not to this list.
  }
}

Selector::Selector() {}

Selector::Selector(const SelectorProto& selector_proto)
    : proto(selector_proto) {}

Selector::~Selector() = default;

Selector::Selector(Selector&& other) noexcept = default;
Selector::Selector(const Selector& other) = default;
Selector& Selector::operator=(const Selector& other) = default;
Selector& Selector::operator=(Selector&& other) noexcept = default;

bool Selector::operator<(const Selector& other) const {
  return proto.filters() < other.proto.filters();
}

bool Selector::operator==(const Selector& other) const {
  return !(*this < other) && !(other < *this);
}

Selector& Selector::MustBeVisible() {
  int filter_count = proto.filters().size();
  if (filter_count == 0 || proto.filters(filter_count - 1).has_bounding_box()) {
    // Avoids adding duplicate visibility requirements in the common case.
    return *this;
  }
  proto.add_filters()->mutable_bounding_box();
  return *this;
}

bool Selector::empty() const {
  if (base::ranges::any_of(proto.filters(),
                           [](const SelectorProto::Filter& filter) {
                             return filter.filter_case() ==
                                    SelectorProto::Filter::FILTER_NOT_SET;
                           })) {
    return true;
  }

  int semantic_selector_count = base::ranges::count_if(
      proto.filters(), [](const SelectorProto::Filter& filter) {
        return filter.filter_case() == SelectorProto::Filter::kSemantic;
      });
  if (semantic_selector_count > 0) {
    return semantic_selector_count > 1 ||
           proto.filters(0).filter_case() != SelectorProto::Filter::kSemantic;
  }

  return !base::ranges::any_of(
      proto.filters(), [](const SelectorProto::Filter& filter) {
        return filter.filter_case() == SelectorProto::Filter::kCssSelector;
      });
}

std::ostream& operator<<(std::ostream& out, const Selector& selector) {
  return out << selector.proto;
}

#ifndef NDEBUG
namespace {

// Debug output for pseudo types.
std::ostream& operator<<(std::ostream& out, PseudoType pseudo_type) {
  return out << PseudoTypeName(pseudo_type);
}

std::ostream& operator<<(std::ostream& out, const TextFilter& c) {
  out << "/" << c.re2() << "/";
  if (c.case_sensitive()) {
    out << "i";
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const AutofillValueRegexp& c) {
  out << "/"
      << field_formatter::GetHumanReadableValueExpression(
             c.value_expression_re2().value_expression())
      << "/";
  if (c.value_expression_re2().case_sensitive()) {
    out << "i";
  }
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const SelectorProto::PropertyFilter& c) {
  out << c.property() << " ~= ";
  switch (c.value_case()) {
    case SelectorProto::PropertyFilter::kTextFilter:
      out << c.text_filter();
      break;
    case SelectorProto::PropertyFilter::kAutofillValueRegexp:
      out << c.autofill_value_regexp();
      break;
    case SelectorProto::PropertyFilter::VALUE_NOT_SET:
      out << "/<unknown>/";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const SelectorProto::SemanticFilter& c) {
  out << "Semantic { role: " << c.role() << ", objective: ";
  if (c.ignore_objective()) {
    out << "ignored";
  } else {
    out << c.objective();
  }
  out << " }";
  return out;
}

std::ostream& operator<<(
    std::ostream& out,
    const google::protobuf::RepeatedPtrField<SelectorProto::Filter>& filters) {
  out << "[";
  std::string separator = "";
  for (const SelectorProto::Filter& filter : filters) {
    out << separator << filter;
    separator = " ";
  }
  out << "]";
  return out;
}
}  // namespace
#endif  // NDEBUG

std::ostream& operator<<(std::ostream& out, const SelectorProto& selector) {
#ifdef NDEBUG
  out << selector.filters().size() << " filter(s)";
#else
  out << selector.filters();
#endif  // NDEBUG
  return out;
}

std::ostream& operator<<(std::ostream& out, const SelectorProto::Filter& f) {
#ifdef NDEBUG
  // DEBUG output not available.
  return out << "filter case=" << f.filter_case();
#else
  switch (f.filter_case()) {
    case SelectorProto::Filter::kEnterFrame:
      out << "/";
      return out;

    case SelectorProto::Filter::kCssSelector:
      out << f.css_selector();
      return out;

    case SelectorProto::Filter::kInnerText:
      out << "innerText~=" << f.inner_text();
      return out;

    case SelectorProto::Filter::kValue:
      out << "value~=" << f.value();
      return out;

    case SelectorProto::Filter::kPseudoType:
      out << "::" << f.pseudo_type();
      return out;

    case SelectorProto::Filter::kPseudoElementContent:
      out << "::" << f.pseudo_element_content().pseudo_type()
          << "~=" << f.pseudo_element_content().content();
      return out;

    case SelectorProto::Filter::kCssStyle:
      if (!f.css_style().pseudo_element().empty()) {
        out << f.css_style().pseudo_element() << " ";
      }
      out << "style." << f.css_style().property()
          << "~=" << f.css_style().value();
      return out;

    case SelectorProto::Filter::kBoundingBox:
      if (f.bounding_box().require_nonempty()) {
        out << "bounding_box (nonempty)";
      } else {
        out << "bounding_box (any)";
      }
      return out;

    case SelectorProto::Filter::kNthMatch:
      out << "nth_match[" << f.nth_match().index() << "]";
      return out;

    case SelectorProto::Filter::kLabelled:
      out << "labelled";
      return out;

    case SelectorProto::Filter::kMatchCssSelector:
      out << "matches: " << f.css_selector();
      return out;

    case SelectorProto::Filter::kOnTop:
      out << "on_top";
      if (!f.on_top().scroll_into_view_if_needed()) {
        out << "(no scroll)";
      }
      if (f.on_top().accept_element_if_not_in_view()) {
        out << "(accept not in view)";
      }
      return out;

    case SelectorProto::Filter::kProperty:
      out << f.property();
      return out;

    case SelectorProto::Filter::kParent:
      out << "parent";
      return out;

    case SelectorProto::Filter::kSemantic:
      out << f.semantic();
      return out;

    case SelectorProto::Filter::FILTER_NOT_SET:
      // Either unset or set to an unsupported value. Let's assume the worse.
      out << "INVALID";
      return out;
  }
#endif  // NDEBUG
}

}  // namespace autofill_assistant
