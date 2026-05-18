// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FOCUS_BROWSER_FOCUS_CONTROLLER_H_
#define CHROME_BROWSER_UI_FOCUS_BROWSER_FOCUS_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

namespace ui {
class BaseWindow;
}

// Manages pane focus rotation and accessibility focus for its associated
// Browser window. This is a virtual base class.
class BrowserFocusController {
 public:
  DECLARE_USER_DATA(BrowserFocusController);

  virtual ~BrowserFocusController();

  BrowserFocusController(const BrowserFocusController&) = delete;
  BrowserFocusController& operator=(const BrowserFocusController&) = delete;

  static BrowserFocusController* From(BrowserWindowInterface* browser);
  static const BrowserFocusController* From(
      const BrowserWindowInterface* browser);

  // Rotates pane focus forwards or backwards.
  void RotatePaneFocus(bool forwards);

  // Focuses the active web contents pane.
  virtual void FocusWebContentsPane() = 0;

  // Focuses the first inactive popup or infobar for accessibility.
  virtual void FocusInactivePopupForAccessibility() = 0;

  // Helper to activate the first inactive bubble for accessibility.
  // Returns true if a bubble was activated.
  virtual bool ActivateFirstInactiveBubbleForAccessibility() = 0;

 protected:
  BrowserFocusController(ui::BaseWindow* base_window,
                         ui::UnownedUserDataHost& host);

 private:
  const raw_ref<ui::BaseWindow> base_window_;
  ui::ScopedUnownedUserData<BrowserFocusController> scoped_data_holder_;
};

// A stub implementation of BrowserFocusController that does nothing. Used as a
// fallback when no concrete implementation is available (e.g. in tests).
class StubBrowserFocusController : public BrowserFocusController {
 public:
  StubBrowserFocusController(ui::BaseWindow* base_window,
                             ui::UnownedUserDataHost& host);
  ~StubBrowserFocusController() override;

  // BrowserFocusController:
  void FocusWebContentsPane() override;
  void FocusInactivePopupForAccessibility() override;
  bool ActivateFirstInactiveBubbleForAccessibility() override;
};

#endif  // CHROME_BROWSER_UI_FOCUS_BROWSER_FOCUS_CONTROLLER_H_
