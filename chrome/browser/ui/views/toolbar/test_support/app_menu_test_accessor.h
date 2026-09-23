// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TEST_SUPPORT_APP_MENU_TEST_ACCESSOR_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TEST_SUPPORT_APP_MENU_TEST_ACCESSOR_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "ui/views/controls/menu/menu_runner.h"

class AppMenu;
class AppMenuControl;
class AppMenuModel;
class BrowserWindowInterface;

namespace ui {
struct AXActionData;
class ElementIdentifier;
}  // namespace ui

namespace views {
class BubbleAnchor;
class MenuItemView;
}  // namespace views

// Test accessor for interacting with and inspecting the app menu, regardless of
// whether it is backed by Views (BrowserAppMenuButton) or WebUI
// (WebUIAppMenuControl).
class AppMenuTestAccessor {
 public:
  explicit AppMenuTestAccessor(BrowserWindowInterface* browser);
  explicit AppMenuTestAccessor(AppMenuControl* control);
  AppMenuTestAccessor(const AppMenuTestAccessor&) = default;
  AppMenuTestAccessor& operator=(const AppMenuTestAccessor&) = default;
  ~AppMenuTestAccessor();

  // Shows the app menu.
  void ShowMenu(int run_types = views::MenuRunner::NO_FLAGS);

  // Closes the app menu if it is currently showing.
  void CloseMenu();

  // Returns true if the app menu is currently showing.
  bool IsMenuShowing() const;

  // Returns the AppMenu instance, or nullptr if none exists.
  AppMenu* GetAppMenu() const;

  // Returns the AppMenuModel instance, or nullptr if none exists.
  AppMenuModel* GetAppMenuModel() const;

  // Returns the root MenuItemView of the open menu, or nullptr if not showing.
  views::MenuItemView* GetRootMenuItemView() const;

  // Executes a command in the app menu with the given command ID and mouse
  // event flags.
  void ExecuteCommand(int command_id, int mouse_event_flags = 0);

  // Returns true if the item corresponding to `element_id` is currently
  // alerted/highlighted.
  bool IsElementIdAlerted(ui::ElementIdentifier element_id) const;

  // Sets the timer for the menu, used to test menu duration / timeout metrics.
  void SetMenuTimerForTesting(base::ElapsedTimer timer);

  // Handles an accessibility action (e.g. kExpand or kCollapse).
  bool HandleAccessibleAction(const ui::AXActionData& action_data);

  // Returns the bubble anchor for the app menu.
  views::BubbleAnchor GetAnchor() const;

  // Returns the underlying AppMenuControl.
  AppMenuControl* GetControl() const { return control_; }

 private:
  raw_ptr<AppMenuControl> control_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TEST_SUPPORT_APP_MENU_TEST_ACCESSOR_H_
