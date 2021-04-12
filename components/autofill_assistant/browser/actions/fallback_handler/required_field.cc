// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/fallback_handler/required_field.h"

namespace autofill_assistant {

RequiredField::RequiredField() = default;

RequiredField::~RequiredField() = default;

RequiredField::RequiredField(const RequiredField& copy) = default;

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
