// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_ACTIONS_H_
#define CHROME_BROWSER_UI_BROWSER_ACTIONS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class Profile;

namespace actions {
class ActionItem;
}  // namespace actions

// Actions that a user can take that are scoped to a browser window.
class BrowserActions {
 public:
  DECLARE_USER_DATA(BrowserActions);

  static BrowserActions* From(BrowserWindowInterface* browser);
  static const BrowserActions* From(const BrowserWindowInterface* browser);

  explicit BrowserActions(BrowserWindowInterface* bwi);
  BrowserActions(const BrowserActions&) = delete;
  BrowserActions& operator=(const BrowserActions&) = delete;
  ~BrowserActions();

  actions::ActionItem* root_action_item() const { return root_action_item_; }

  // Initialization is separate from construction to allow more precise timing.
  void InitializeBrowserActions();

  void set_root_action_item_for_testing(actions::ActionItem* item) {
    root_action_item_ = item;
  }

 private:
  // Helper functions to initialize actions grouped roughly by their type.
  void InitializeSidePanelActions();
  void InitializePageActionIconActions();
  void InitializeChromeMenuActions();
  void InitializeToolbarAndMiscActions();
  void InitializeNavigationActions();
  void InitializeSubmenuActions();

  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  const raw_ref<BrowserWindowInterface> bwi_;
  const raw_ref<Profile> profile_;
  ui::ScopedUnownedUserData<BrowserActions> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_ACTIONS_H_
