// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FOCUS_BROWSER_FOCUS_CONTROLLER_H_
#define CHROME_BROWSER_UI_FOCUS_BROWSER_FOCUS_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

// Manages pane focus rotation and accessibility focus for its associated
// Browser window.
class BrowserFocusController {
 public:
  DECLARE_USER_DATA(BrowserFocusController);

  explicit BrowserFocusController(BrowserWindowInterface& browser);
  ~BrowserFocusController();

  BrowserFocusController(const BrowserFocusController&) = delete;
  BrowserFocusController& operator=(const BrowserFocusController&) = delete;

  static BrowserFocusController* From(BrowserWindowInterface* browser);
  static const BrowserFocusController* From(
      const BrowserWindowInterface* browser);

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
  const raw_ref<BrowserWindowInterface> browser_;

  ui::ScopedUnownedUserData<BrowserFocusController> scoped_data_holder_;
};

#endif  // CHROME_BROWSER_UI_FOCUS_BROWSER_FOCUS_CONTROLLER_H_
