// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PIN_TEXTFIELD_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PIN_TEXTFIELD_H_

#include <memory>
#include <vector>

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/view.h"

namespace ui {
struct AXNodeData;
}  // namespace ui

// Implements textfield for entering a PIN number with custom drawing logic for
// displaying each digit in a separate cell.
class PinTextfield : public views::Textfield {
  METADATA_HEADER(PinTextfield, views::View)

 public:
  explicit PinTextfield(int pin_digits_count);
  ~PinTextfield() override;

  // Appends a digit to the next free pin cell. Does nothing if all pin digits
  // are already typed. Returns true if a digit was appended.
  bool AppendDigit(std::u16string digit);

  // Removes a digit from the last cell. Does nothing if no digits are typed.
  // Returns true if a digit was removed.
  bool RemoveDigit();

  // Returns currently typed pin.
  std::u16string GetPin();
  void SetPin(const std::u16string& pin);

  void SetObscured(bool obscured);
  void SetDisabled(bool disabled);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 protected:
  // views::Textfield:
  void UpdateAccessibleTextSelection() override;

 private:
  // Returns true for the first empty cell or the last cell when the full pin is
  // typed (when the whole view has focus).
  bool HasCellFocus(int cell) const;

  // Updates the current selection and notifies that it changed along with the
  // pin value.
  void UpdateAccessibilityAfterPinChange();

  // Updates text color based on the current state of `disabled_`.
  void UpdateTextColor();

  // Render text for each of the pin cells.
  std::vector<std::unique_ptr<gfx::RenderText>> render_texts_;

  // Amount of pin cells.
  const int pin_digits_count_;

  // Amount of digits that are currently typed.
  int digits_typed_count_ = 0;

  // Whether entering pin is currently disabled.
  bool disabled_ = false;

  // Whether the pin characters are currently obscured.
  bool obscured_ = true;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_PIN_TEXTFIELD_H_
