// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_ACTIONS_H_
#define CHROME_BROWSER_UI_BROWSER_ACTIONS_H_

#include <memory>

#include "base/callback_list.h"

class Browser;
class BrowserActionPrefsListener;
class TranslateBrowserActionListener;

namespace actions {
class ActionItem;
}  // namespace actions

// Actions that a user can take that are scoped to a browser window.
class BrowserActions {
 public:
  explicit BrowserActions(Browser& browser);
  BrowserActions(const BrowserActions&) = delete;
  BrowserActions& operator=(const BrowserActions&) = delete;
  ~BrowserActions();

  static std::u16string GetCleanTitleAndTooltipText(std::u16string string);

  actions::ActionItem* root_action_item() const { return root_action_item_; }

  // Initialization is separate from construction to allow more precise timing.
  void InitializeBrowserActions();

  void RemoveListeners();

 private:
  // Creates all the listeners for the action items that update different states
  // and property of the action item.
  void AddListeners();

  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  std::unique_ptr<TranslateBrowserActionListener>
      translate_browser_action_listener_;
  std::unique_ptr<BrowserActionPrefsListener> browser_action_prefs_listener_;
  const raw_ref<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_ACTIONS_H_
