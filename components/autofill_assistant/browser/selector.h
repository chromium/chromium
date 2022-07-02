// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SELECTOR_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SELECTOR_H_

#include <ostream>
#include <string>
#include <vector>

#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// Convenience functions for creating SelectorProtos.
SelectorProto ToSelectorProto(const std::string& s);
SelectorProto ToSelectorProto(const std::vector<std::string>& s);

// Returns the CSS name of a pseudo-type, without "::" prefix.
std::string PseudoTypeName(PseudoType pseudoType);

// Convenience wrapper around a SelectorProto that makes it simpler to work with
// selectors.
//
// Selectors are comparables, can be used as std::map key or std::set elements
// and converted to string with operator<<.
struct Selector {
  SelectorProto proto;

  Selector();
  ~Selector();

  explicit Selector(const SelectorProto& proto);
  explicit Selector(const std::vector<std::string>& s)
      : Selector(ToSelectorProto(s)) {}

  Selector(Selector&& other) noexcept;
  Selector(const Selector& other);
  Selector& operator=(Selector&& other) noexcept;
  Selector& operator=(const Selector& other);

  bool operator<(const Selector& other) const;
  bool operator==(const Selector& other) const;

  // Convenience function to update the visible field in a fluent style.
  Selector& MustBeVisible();

  // Checks whether this selector is empty or invalid.
  // TODO(b/235308082): Rename this to be more appropriate to what the method
  // actually does.
  bool empty() const;

  // Convenience function to set inner_text_pattern in a fluent style.
  Selector& MatchingInnerText(const std::string& pattern) {
    return MatchingInnerText(pattern, false);
  }

  // Convenience function  to set inner_text_pattern matching with case
  // sensitivity.
  Selector& MatchingInnerText(const std::string& pattern, bool case_sensitive) {
    auto* text_filter = proto.add_filters()->mutable_inner_text();
    text_filter->set_re2(pattern);
    text_filter->set_case_sensitive(case_sensitive);
    return *this;
  }

  // Convenience function to set inner_text_pattern in a fluent style.
  Selector& MatchingValue(const std::string& pattern) {
    return MatchingValue(pattern, false);
  }

  // Convenience function to set value_pattern matchinng with case sensitivity.
  Selector& MatchingValue(const std::string& pattern, bool case_sensitive) {
    auto* text_filter = proto.add_filters()->mutable_value();
    text_filter->set_re2(pattern);
    text_filter->set_case_sensitive(case_sensitive);
    return *this;
  }

  Selector& SetPseudoType(PseudoType pseudo_type) {
    proto.add_filters()->set_pseudo_type(pseudo_type);
    return *this;
  }
};

// Debug output operator for selectors. The output is only useful in
// debug builds.
std::ostream& operator<<(std::ostream& out, const Selector& selector);

// Debug output for selector protos. The output is only useful in
// debug builds.
std::ostream& operator<<(std::ostream& out, const SelectorProto& proto);

// Debug output for selector filter protos. The output is only useful in
// debug builds.
std::ostream& operator<<(std::ostream& out,
                         const SelectorProto::Filter& filter);

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SELECTOR_H_
