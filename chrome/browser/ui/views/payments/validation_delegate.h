// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATION_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATION_DELEGATE_H_

#include <string>


namespace views {
class Textfield;
}  // namespace views

namespace payments {

class ValidatingCombobox;

// Handles text field validation and formatting.
class ValidationDelegate {
 public:
  virtual ~ValidationDelegate();

  virtual bool ShouldFormat();
  virtual std::u16string Format(const std::u16string& text);

  // Only the delegate knows how to validate the input fields.
  virtual bool IsValidTextfield(views::Textfield* textfield,
                                std::u16string* error_message) = 0;
  virtual bool IsValidCombobox(ValidatingCombobox* combobox,
                               std::u16string* error_message) = 0;

  // Notifications to let delegate react to input field changes and also let
  // caller know if the new values are valid. |was_blurred| indicates if the
  // field has yet to be blurred once by the user.
  virtual bool TextfieldValueChanged(views::Textfield* textfield,
                                     bool was_blurred) = 0;
  virtual bool ComboboxValueChanged(ValidatingCombobox* combobox) = 0;

  // Lets the delegate know that the model of the combobox has changed, e.g.,
  // when it gets filled asynchronously as for the state field.
  virtual void ComboboxModelChanged(ValidatingCombobox* combobox) = 0;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_VALIDATION_DELEGATE_H_
