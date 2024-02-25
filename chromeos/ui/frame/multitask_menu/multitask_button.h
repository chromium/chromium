// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_BUTTON_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace chromeos {

// The base button for multitask menu to create Full Screen and Float buttons.
class MultitaskButton : public views::Button {
  METADATA_HEADER(MultitaskButton, views::Button)

 public:
  // The types of single operated multitask button.
  enum class Type {
    kFull,   // The button that turn the window to full screen mode.
    kFloat,  // The button that float the window.
  };

  MultitaskButton(PressedCallback callback,
                  Type type,
                  bool is_portrait_mode,
                  bool paint_as_active,
                  const std::u16string& name);

  MultitaskButton(const MultitaskButton&) = delete;
  MultitaskButton& operator=(const MultitaskButton&) = delete;
  ~MultitaskButton() override = default;

  // views::Button:
  void StateChanged(views::Button::ButtonState old_state) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

 private:
  const Type type_;
  // The display orientation. This determines whether button is in
  // landscape/portrait mode.
  const bool is_portrait_mode_;

  // Used to determine whether the button should be painted as active. If a
  // window is in fullscreen or floated state, it should be painted as active.
  const bool paint_as_active_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_BUTTON_H_
