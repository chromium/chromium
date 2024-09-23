// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_OVERFLOW_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_OVERFLOW_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_runner.h"

class ToolbarController;

// A chevron button that indicates some toolbar elements have overflowed due to
// browser window being smaller than usual. Left press on it displays a drop
// down list of overflowed elements.
class OverflowButton : public ToolbarButton {
  METADATA_HEADER(OverflowButton, ToolbarButton)

 public:

  OverflowButton();
  OverflowButton(const OverflowButton&) = delete;
  OverflowButton& operator=(const OverflowButton&) = delete;
  ~OverflowButton() override;

  // Triggered by left press.
  void RunMenu();

  views::MenuButtonController* menu_button_controller() {
    return menu_button_controller_;
  }
  void set_toolbar_controller(ToolbarController* toolbar_controller) {
    toolbar_controller_ = toolbar_controller;
  }

 private:
  raw_ptr<views::MenuButtonController> menu_button_controller_;
  // The controller is used for handling the click event and should be set
  // before `RunMenu()` can be called. This is owned by the ToolbarView which is
  // also the parent of `this`.
  raw_ptr<ToolbarController> toolbar_controller_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_OVERFLOW_BUTTON_H_
