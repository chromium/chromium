// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_ACTIONS_H_
#define CHROME_BROWSER_UI_BROWSER_ACTIONS_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"

class BrowserActionPrefsListener;
class BrowserWindowInterface;
class Profile;

namespace actions {
class ActionItem;
}  // namespace actions

// Actions that a user can take that are scoped to a browser window.
class BrowserActions {
 public:
  explicit BrowserActions(BrowserWindowInterface* bwi);
  BrowserActions(const BrowserActions&) = delete;
  BrowserActions& operator=(const BrowserActions&) = delete;
  ~BrowserActions();

  static std::u16string GetCleanTitleAndTooltipText(std::u16string string);

  actions::ActionItem* root_action_item() const { return root_action_item_; }

  // Initialization is separate from construction to allow more precise timing.
  void InitializeBrowserActions();

 private:
  // Creates all the listeners for the action items that update different states
  // and property of the action item.
  void AddListeners();

  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  std::unique_ptr<BrowserActionPrefsListener> browser_action_prefs_listener_;
  const raw_ref<BrowserWindowInterface> bwi_;
  const raw_ref<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_ACTIONS_H_
