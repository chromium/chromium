// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/selector.h"

#include "base/strings/string_util.h"

namespace autofill_assistant {

Selector::Selector() {}

Selector::Selector(const ElementReferenceProto& proto) {
  for (const auto& selector : proto.selectors()) {
    selectors.emplace_back(selector);
  }
  must_be_visible = proto.visibility_requirement() == MUST_BE_VISIBLE;
  inner_text_pattern = proto.inner_text_pattern();
  value_pattern = proto.value_pattern();
  pseudo_type = proto.pseudo_type();
}

Selector::Selector(std::vector<std::string> s)
    : selectors(s), pseudo_type(PseudoType::UNDEFINED) {}
Selector::Selector(std::vector<std::string> s, PseudoType p)
    : selectors(s), pseudo_type(p) {}
Selector::~Selector() = default;

Selector::Selector(Selector&& other) = default;
Selector::Selector(const Selector& other) = default;
Selector& Selector::operator=(const Selector& other) = default;
Selector& Selector::operator=(Selector&& other) = default;

bool Selector::operator<(const Selector& other) const {
  return std::tie(selectors, inner_text_pattern, value_pattern, must_be_visible,
                  pseudo_type) <
         std::tie(other.selectors, other.inner_text_pattern,
                  other.value_pattern, other.must_be_visible,
                  other.pseudo_type);
}

bool Selector::operator==(const Selector& other) const {
  return selectors == other.selectors &&
         inner_text_pattern == other.inner_text_pattern &&
         value_pattern == other.value_pattern &&
         must_be_visible == other.must_be_visible &&
         pseudo_type == other.pseudo_type;
}

bool Selector::empty() const {
  return this->selectors.empty();
}

std::ostream& operator<<(std::ostream& out, PseudoType pseudo_type) {
#ifdef NDEBUG
  return out << static_cast<int>(pseudo_type);
#else
  switch (pseudo_type) {
    case UNDEFINED:
      out << "UNDEFINED";
      break;
    case FIRST_LINE:
      out << "FIRST_LINE";
      break;
    case FIRST_LETTER:
      out << "FIRST_LETTER";
      break;
    case BEFORE:
      out << "BEFORE";
      break;
    case AFTER:
      out << "AFTER";
      break;
    case BACKDROP:
      out << "BACKDROP";
      break;
    case SELECTION:
      out << "SELECTION";
      break;
    case FIRST_LINE_INHERITED:
      out << "FIRST_LINE_INHERITED";
      break;
    case SCROLLBAR:
      out << "SCROLLBAR";
      break;
    case SCROLLBAR_THUMB:
      out << "SCROLLBAR_THUMB";
      break;
    case SCROLLBAR_BUTTON:
      out << "SCROLLBAR_BUTTON";
      break;
    case SCROLLBAR_TRACK:
      out << "SCROLLBAR_TRACK";
      break;
    case SCROLLBAR_TRACK_PIECE:
      out << "SCROLLBAR_TRACK_PIECE";
      break;
    case SCROLLBAR_CORNER:
      out << "SCROLLBAR_CORNER";
      break;
    case RESIZER:
      out << "RESIZER";
      break;
    case INPUT_LIST_BUTTON:
      out << "INPUT_LIST_BUTTON";
      break;

      // Intentionally no default case to make compilation fail if a new value
      // was added to the enum but not to this list.
  }
  return out;
#endif
}
std::ostream& operator<<(std::ostream& out, const Selector& selector) {
#ifdef NDEBUG
  out << selector.selectors.size() << " element(s)";
  return out;
#else
  out << "elements=[" << base::JoinString(selector.selectors, ",") << "]";
  if (!selector.inner_text_pattern.empty()) {
    out << " innerText =~ /";
    out << selector.inner_text_pattern;
    out << "/";
  }
  if (!selector.value_pattern.empty()) {
    out << " value =~ /";
    out << selector.value_pattern;
    out << "/";
  }
  if (selector.must_be_visible) {
    out << " must_be_visible";
  }
  if (selector.pseudo_type != PseudoType::UNDEFINED) {
    out << "::" << selector.pseudo_type;
  }
  return out;
#endif  // NDEBUG
}

}  // namespace autofill_assistant
