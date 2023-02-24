// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTROLS_OBSCURABLE_LABEL_WITH_TOGGLE_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_CONTROLS_OBSCURABLE_LABEL_WITH_TOGGLE_BUTTON_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class Label;
class ToggleImageButton;
}  // namespace views

// Creates a view with label and eye icon button that displays the obscured or
// unobscured label on clicking.
class ObscurableLabelWithToggleButton : public views::BoxLayoutView {
 public:
  METADATA_HEADER(ObscurableLabelWithToggleButton);
  ObscurableLabelWithToggleButton(
      const std::u16string& obscured_value,
      const std::u16string& revealed_value,
      const std::u16string& toggle_button_tooltip,
      const std::u16string& toggle_button_toggled_tooltip);
  ObscurableLabelWithToggleButton(const ObscurableLabelWithToggleButton&) =
      delete;
  ObscurableLabelWithToggleButton& operator=(
      const ObscurableLabelWithToggleButton&) = delete;
  ~ObscurableLabelWithToggleButton() override;

  views::Label* value();
  views::ToggleImageButton* toggle_obscured();

 private:
  // Toggles between the obscured and revealed values.
  void ToggleValueObscured();

  const std::u16string obscured_value_;
  const std::u16string revealed_value_;

  raw_ptr<views::Label> value_ = nullptr;
  // The button that toggles whether the value is obscured.
  raw_ptr<views::ToggleImageButton> toggle_obscured_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONTROLS_OBSCURABLE_LABEL_WITH_TOGGLE_BUTTON_H_
