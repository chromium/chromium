// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/fallback_handler/required_field.h"

namespace autofill_assistant {

RequiredField::RequiredField() = default;

RequiredField::~RequiredField() = default;

RequiredField::RequiredField(const RequiredField& copy) = default;

void RequiredField::FromProto(const RequiredFieldProto& required_field_proto) {
  selector = Selector(required_field_proto.element());
  proto = required_field_proto;
}

bool RequiredField::ShouldFallback(bool apply_fallback) const {
  return (status == EMPTY && HasValue() &&
          !proto.has_option_element_to_click() &&
          !(proto.is_optional() && !apply_fallback)) ||
         (status != EMPTY && !HasValue() &&
          !proto.has_option_element_to_click()) ||
         (proto.forced() && apply_fallback) ||
         (proto.has_option_element_to_click() && apply_fallback);
}

bool RequiredField::HasValue() const {
  return !proto.value_expression().chunk().empty() ||
         proto.has_option_comparison_value_expression_re2();
}

}  // namespace autofill_assistant
