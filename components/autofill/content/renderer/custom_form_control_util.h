// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_UTIL_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_UTIL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "components/autofill/core/common/form_field_data.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_element.h"

namespace autofill {

// Represents a custom form control detected using `detectCustomFormControls()`
// from a custom form control script.
// TODO(crbug.com/581337039): Support `is_placeholder` and `static_options`.
struct CustomFormControl {
  friend bool operator==(const CustomFormControl&,
                         const CustomFormControl&) = default;

  // The DOM element whose subtree identifies the custom form control.
  blink::WebElement canonical_dom_node;
  // The `FieldType` of the control, as classified by the script.
  std::u16string field_type;
  // Whether the control can currently be interacted with.
  bool is_active = false;
  // The label associated with the control, as identified by the script.
  std::optional<std::u16string> label;
  // The currently selected option, if any.
  std::optional<SelectOption> current_option;
};

// The options of a custom select, as returned by the manager's
// `extractOptions()`.
struct CustomFormControlOptions {
  friend bool operator==(const CustomFormControlOptions&,
                         const CustomFormControlOptions&) = default;

  std::vector<SelectOption> options;
  // Whether the manager opened a popover to extract the options and left it
  // open. Selecting an option with `setValue()` closes the popover. If no
  // option is selected, `CustomFormControlScriptExecutor::ClosePopover()` must
  // close it instead.
  bool popover_needs_to_be_closed = false;
};

// The outcome of the manager's `setValue()`.
enum class CustomFormControlSetValueResult {
  // The option was selected, and the script verified it in the DOM.
  kOk,
  // The control has no option that matches the value and text. This is a
  // mismatch with the data to fill, not a script failure.
  kOptionNotFound,
  // The script couldn't find, open or update the control. Also used if the
  // call fails or returns an invalid result.
  kExecutionError,
};

// Groups `controls` by their outermost ancestor `<form>` element. Unowned
// controls are grouped under a null `blink::WebFormElement()`. Relative order
// of `controls` is preserved within each form.
base::flat_map<blink::WebFormElement, std::vector<CustomFormControl>>
GroupCustomFormControlsByForm(std::vector<CustomFormControl> controls);

// Converts a `CustomFormControl` to a `FormFieldData`.
FormFieldData CustomFormControlToFormField(const CustomFormControl& control);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_CUSTOM_FORM_CONTROL_UTIL_H_
