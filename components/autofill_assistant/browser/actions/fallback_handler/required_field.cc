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
  value_expression = required_field_proto.value_expression();
  forced = required_field_proto.forced();
  optional = required_field_proto.is_optional();
  fill_strategy = required_field_proto.fill_strategy();
  delay_in_millisecond = required_field_proto.delay_in_millisecond();
  select_strategy = required_field_proto.select_strategy();

  if (required_field_proto.has_option_element_to_click()) {
    fallback_click_element =
        Selector(required_field_proto.option_element_to_click());
    click_type = required_field_proto.click_type();
  }
}

bool RequiredField::ShouldFallback(bool apply_fallback) const {
  return (status == EMPTY && !value_expression.empty() &&
          !fallback_click_element.has_value() &&
          !(optional && !apply_fallback)) ||
         (status != EMPTY && value_expression.empty() &&
          !fallback_click_element.has_value()) ||
         (forced && apply_fallback) ||
         (fallback_click_element.has_value() && apply_fallback);
}

}  // namespace autofill_assistant
