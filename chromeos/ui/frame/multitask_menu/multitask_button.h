// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_BUTTON_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_BUTTON_H_

#include "ui/views/controls/button/button.h"

namespace chromeos {

// The base button for multitask menu to create Full Screen and Float buttons.
class MultitaskBaseButton : public views::Button {
 public:
  // The types of single operated multitask button.
  enum class Type {
    kFull,   // The button that turn the window to full screen mode.
    kFloat,  // The button that float the window.
  };

  MultitaskBaseButton(PressedCallback callback,
                      Type type,
                      const std::u16string& name);

  MultitaskBaseButton(const MultitaskBaseButton&) = delete;
  MultitaskBaseButton& operator=(const MultitaskBaseButton&) = delete;
  ~MultitaskBaseButton() override = default;

  // views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 private:
  const Type type_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_BUTTON_H_
