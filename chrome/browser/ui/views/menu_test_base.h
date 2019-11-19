// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MENU_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_MENU_TEST_BASE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ui/views/test/view_event_test_base.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_delegate.h"

namespace views {
class Button;
class MenuItemView;
class MenuRunner;
}

// This is a convenience base class for menu related tests to provide some
// common functionality.
//
// Subclasses should implement:
//  BuildMenu()            populate the menu
//  DoTestWithMenuOpen()   initiate the test
//
// Subclasses can call:
//  Click()       to post a mouse click on a View
//  KeyPress()    to post a key press
//
// Although it should be possible to post a menu multiple times,
// MenuItemView prevents repeated activation of a menu by clicks too
// close in time.
class MenuTestBase : public ViewEventTestBase,
                     public views::ButtonListener,
                     public views::MenuDelegate {
 public:
  MenuTestBase();
  ~MenuTestBase() override;

  // Generate a mouse click and run |next| once the event has been processed.
  virtual void Click(views::View* view, base::OnceClosure next);

  // Generate a keypress and run |next| once the event has been processed.
  void KeyPress(ui::KeyboardCode keycode, base::OnceClosure next);

  views::MenuItemView* menu() {
    return menu_;
  }

  int last_command() const {
    return last_command_;
  }

 protected:
  views::MenuRunner* menu_runner() { return menu_runner_.get(); }

  // Called to populate the menu.
  virtual void BuildMenu(views::MenuItemView* menu) = 0;

  // Called once the menu is open.
  virtual void DoTestWithMenuOpen() = 0;

  // Returns a bitmask of flags to use when creating the |menu_runner_|. By
  // default, this is only views::MenuRunner::HAS_MNEMONICS.
  virtual int GetMenuRunnerFlags();

  // ViewEventTestBase implementation.
  void SetUp() override;
  void TearDown() override;
  views::View* CreateContentsView() override;
  void DoTestOnMessageLoop() override;
  gfx::Size GetPreferredSizeForContents() const override;

  // views::ButtonListener implementation
  void ButtonPressed(views::Button* source, const ui::Event& event) override;

  // views::MenuDelegate implementation
  void ExecuteCommand(int id) override;

 private:
  views::MenuButton* button_;
  views::MenuItemView* menu_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // The command id of the last pressed menu item since the menu was opened.
  int last_command_;

  DISALLOW_COPY_AND_ASSIGN(MenuTestBase);
};

#endif  // CHROME_BROWSER_UI_VIEWS_MENU_TEST_BASE_H_
