// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_CONTROL_H_

#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class AppMenuButtonObserver;
namespace views {
class AccessiblePaneView;
class DialogDelegate;
class View;
}

// Interface for controlling the app menu, which may be implemented by a Views
// button or a WebUI-based control.
class AppMenuControl {
 public:
  virtual ~AppMenuControl() = default;

  // Returns the bubble anchor for the app menu.
  virtual views::BubbleAnchor GetAnchor() = 0;

  // Returns true if the control is drawn on screen.
  virtual bool IsDrawn() const = 0;

  // Returns true if the app menu is currently showing.
  virtual bool IsMenuShowing() const = 0;

  // Returns the dialog delegate for any dialog currently anchored to the app
  // menu, or nullptr if none exists.
  virtual views::DialogDelegate* GetDialogDelegate() = 0;

  // Closes the app menu if it is currently showing.
  virtual void CloseMenu() = 0;

  // Shows the app menu.
  virtual void ShowMenu() = 0;

  // Adds or removes an observer to be notified of app menu events.
  virtual void AddObserver(AppMenuButtonObserver* observer) = 0;
  virtual void RemoveObserver(AppMenuButtonObserver* observer) = 0;

  // Returns true if the app menu button is focused.
  virtual bool HasFocus() const = 0;

  // Focuses the app menu button.
  virtual void Focus(views::AccessiblePaneView* pane) = 0;

  // Updates the icon type and severity.
  virtual void SetTypeAndSeverity(
      AppMenuIconController::TypeAndSeverity type_and_severity) = 0;

  // Sets the trailing margin.
  virtual void SetTrailingMargin(int margin) = 0;

  // Returns the views::View that represents this control for pane focusing.
  // - For Views: returns the button itself (AppMenuButton).
  // - For WebUI: returns the parent WebUIToolbarWebView hosting the WebUI.
  virtual views::View* GetFocusablePaneView() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_CONTROL_H_
