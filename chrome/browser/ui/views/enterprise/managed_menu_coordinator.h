// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ENTERPRISE_MANAGED_MENU_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_ENTERPRISE_MANAGED_MENU_COORDINATOR_H_

#include "chrome/browser/ui/browser_user_data.h"
#include "ui/views/view_tracker.h"

// Handles the lifetime and showing/hidden state of the managed menu bubble.
// Owned by the associated browser.
class ManagedMenuCoordinator : public BrowserUserData<ManagedMenuCoordinator> {
 public:
  ~ManagedMenuCoordinator() override;

  // Shows the the profile bubble for this browser.
  void Show();

  // Returns true if the bubble is currently showing for the owning browser.
  bool IsShowing() const;

 private:
  friend class BrowserUserData<ManagedMenuCoordinator>;

  explicit ManagedMenuCoordinator(Browser* browser);

  views::ViewTracker bubble_tracker_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_ENTERPRISE_MANAGED_MENU_COORDINATOR_H_
