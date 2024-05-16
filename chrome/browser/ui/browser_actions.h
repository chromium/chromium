// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_ACTIONS_H_
#define CHROME_BROWSER_UI_BROWSER_ACTIONS_H_

#include "base/callback_list.h"

class Browser;

namespace actions {
class ActionItem;
}  // namespace actions

class BrowserActions {
 public:
  explicit BrowserActions(Browser& browser);
  BrowserActions(const BrowserActions&) = delete;
  BrowserActions& operator=(const BrowserActions&) = delete;
  ~BrowserActions();

  actions::ActionItem* root_action_item() const { return root_action_item_; }

 private:
  void InitializeBrowserActions();

  raw_ptr<actions::ActionItem> root_action_item_ = nullptr;
  const raw_ref<Browser> browser_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_ACTIONS_H_
