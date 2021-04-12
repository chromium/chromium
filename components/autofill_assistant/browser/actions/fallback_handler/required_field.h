// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_FALLBACK_HANDLER_REQUIRED_FIELD_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_FALLBACK_HANDLER_REQUIRED_FIELD_H_

#include "base/optional.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {

// Defines a field that is defined as required in an Autofill action.
struct RequiredField {
 public:
  enum FieldValueStatus { UNKNOWN, EMPTY, NOT_EMPTY };

  ~RequiredField();
  RequiredField();
  RequiredField(const RequiredField& copy);

  template <typename T>
  void FromProto(const T& required_field_proto) {
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

  // The selector of the field that must be filled.
  Selector selector;

  // The value expression to be filled into the field. This gets evaluated with
  // the provided data.
  std::string value_expression;

  // Defines whether the field is currently considered to be filled or not.
  FieldValueStatus status = UNKNOWN;

  // This defines whether or not to overwrite a field initially filled by
  // Autofill in an attempt to fix it. Mostly used in combination with key
  // strokes for fields that e.g. have JavaScript listeners attached.
  bool forced = false;

  // This defines whether or not this field is optional. If it is, missing data
  // or element-not-found errors are not treated as such but rather as FYI. For
  // missing data, the field will be cleared.
  bool optional = false;

  // Keyboard strategy for <input> elements to use. E.g. whether or not to use
  // key strokes.
  KeyboardValueFillStrategy fill_strategy =
      KeyboardValueFillStrategy::UNSPECIFIED_KEYBAORD_STRATEGY;
  // Optional. Only used in combination with a key strokes filling strategy.
  // Adds an artificial delay between each key stroke.
  int delay_in_millisecond = 0;

  // Dropdown strategy for <select> elements to use. E.g. whether to match the
  // label or the value.
  DropdownSelectStrategy select_strategy =
      DropdownSelectStrategy::UNSPECIFIED_SELECT_STRATEGY;

  // For JavaScript driven dropdowns. This defines the option to be clicked.
  // The selector must match a generic option, a |inner_text_pattern| will be
  // attached for matching to a unique option.
  base::Optional<Selector> fallback_click_element = base::nullopt;
  // Optional. The click type to be used for clicking JavaScript driven
  // dropdown elements.
  ClickType click_type = ClickType::NOT_SET;

  // Returns true if fallback is required for this field.
  bool ShouldFallback(bool apply_fallback) const;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_FALLBACK_HANDLER_REQUIRED_FIELD_H_
