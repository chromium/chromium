// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"

class Browser;
class ExtensionsToolbarContainer;

namespace views {
class MenuButtonController;
}  // namespace views

// Button in the toolbar that provides access to the corresponding extensions
// menu.
class ExtensionsToolbarButton : public ToolbarButton,
                                public views::ButtonListener {
 public:
  ExtensionsToolbarButton(Browser* browser,
                          ExtensionsToolbarContainer* extensions_container);

  void UpdateIcon();

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  Browser* const browser_;
  views::MenuButtonController* menu_button_controller_;
  ExtensionsToolbarContainer* const extensions_container_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionsToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_
