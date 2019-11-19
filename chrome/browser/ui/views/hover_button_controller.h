// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HOVER_BUTTON_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_HOVER_BUTTON_CONTROLLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/views/controls/button/button_controller.h"

namespace views {
class ButtonControllerDelegate;
}  // namespace views

// A controller that contains the logic for a button that's the full width of
// its parent.
class HoverButtonController : public views::ButtonController {
 public:
  HoverButtonController(
      views::Button* button,
      views::ButtonListener* listener,
      std::unique_ptr<views::ButtonControllerDelegate> delegate);
  ~HoverButtonController() override;

  // views::ButtonController:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

 private:
  // Listener to be called when button is clicked.
  views::ButtonListener* listener_;

  DISALLOW_COPY_AND_ASSIGN(HoverButtonController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HOVER_BUTTON_CONTROLLER_H_
