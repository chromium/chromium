// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MENU_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_MENU_TEST_BASE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/test/view_event_test_base.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/controls/menu/menu_delegate.h"

namespace views {
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
                     public views::AXEventObserver,
                     public views::MenuDelegate {
 public:
  MenuTestBase();

  MenuTestBase(const MenuTestBase&) = delete;
  MenuTestBase& operator=(const MenuTestBase&) = delete;

  ~MenuTestBase() override;

  // AXEventObserver overrides.
  void OnViewEvent(views::View*, ax::mojom::Event event_type) override;

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
  std::unique_ptr<views::View> CreateContentsView() override;
  void DoTestOnMessageLoop() override;
  gfx::Size GetPreferredSizeForContents() const override;

  // views::MenuDelegate implementation
  void ExecuteCommand(int id) override;

  int GetAXEventCount(ax::mojom::Event event_type) const;

 private:
  void ButtonPressed();

  raw_ptr<views::MenuButton> button_ = nullptr;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  // Owned by `menu_runner_`.
  raw_ptr<views::MenuItemView> menu_ = nullptr;

  // The command id of the last pressed menu item since the menu was opened.
  int last_command_;

  // The number of AX events fired by type.
  static constexpr int kNumEvents =
      static_cast<size_t>(ax::mojom::Event::kMaxValue) + 1;
  std::array<int, kNumEvents> ax_event_counts_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MENU_TEST_BASE_H_
