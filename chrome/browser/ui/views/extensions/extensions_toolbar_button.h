// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_

#include <memory>

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class ExtensionsToolbarContainer;


// Button in the toolbar that provides access to the corresponding extensions
// menu.
class ExtensionsToolbarButton : public ToolbarButton,
                                public views::WidgetObserver {
 public:
  ExtensionsToolbarButton(Browser* browser,
                          ExtensionsToolbarContainer* extensions_container);
  ExtensionsToolbarButton(const ExtensionsToolbarButton&) = delete;
  ExtensionsToolbarButton& operator=(const ExtensionsToolbarButton&) = delete;
  ~ExtensionsToolbarButton() override;

  // ToolbarButton:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  const char* GetClassName() const override;
  void UpdateIcon() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  int GetIconSize() const;

  void ButtonPressed();

  // A lock to keep the button pressed when a popup is visible.
  std::unique_ptr<views::MenuButtonController::PressedLock> pressed_lock_;

  Browser* const browser_;
  views::MenuButtonController* menu_button_controller_;
  ExtensionsToolbarContainer* const extensions_container_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_TOOLBAR_BUTTON_H_
