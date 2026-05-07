// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FOCUS_BROWSER_FOCUS_CONTROLLER_H_
#define CHROME_BROWSER_UI_FOCUS_BROWSER_FOCUS_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace ui {
class BaseWindow;
}

// Manages pane focus rotation and accessibility focus for its associated
// Browser window.
class BrowserFocusController {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Focuses the active web contents pane.
    virtual void FocusWebContentsPane() = 0;

    // Focuses the first inactive popup or infobar for accessibility.
    virtual void FocusInactivePopupForAccessibility() = 0;

    // Helper to activate the first inactive bubble for accessibility.
    // Returns true if a bubble was activated.
    virtual bool ActivateFirstInactiveBubbleForAccessibility() = 0;
  };

  DECLARE_USER_DATA(BrowserFocusController);

  BrowserFocusController(ui::BaseWindow* base_window,
                         ui::UnownedUserDataHost& host);
  ~BrowserFocusController();

  BrowserFocusController(const BrowserFocusController&) = delete;
  BrowserFocusController& operator=(const BrowserFocusController&) = delete;

  static BrowserFocusController* From(BrowserWindowInterface* browser);
  static const BrowserFocusController* From(
      const BrowserWindowInterface* browser);

  void SetDelegate(std::unique_ptr<Delegate> delegate);

  // Rotates pane focus forwards or backwards.
  void RotatePaneFocus(bool forwards);

  // Focuses the active web contents pane.
  void FocusWebContentsPane();

  // Focuses the first inactive popup or infobar for accessibility.
  void FocusInactivePopupForAccessibility();

  // Helper to activate the first inactive bubble for accessibility.
  // Returns true if a bubble was activated.
  bool ActivateFirstInactiveBubbleForAccessibility();

 private:
  const raw_ref<ui::BaseWindow> base_window_;

  std::unique_ptr<Delegate> delegate_;

  ui::ScopedUnownedUserData<BrowserFocusController> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_FOCUS_BROWSER_FOCUS_CONTROLLER_H_
