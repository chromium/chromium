// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_PROXY_ACTION_ITEM_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_PROXY_ACTION_ITEM_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_header_macros.h"

// A representation of an ActionItem used in the block style ChroMenu.
// It references an existing ActionItem registered in the GlobalActionManager or
// BrowserActions. This way we can assign different properties to the
// ActionItems in the ChroMenu without modifying the original ActionItem.
class AppMenuProxyActionItem : public actions::ActionItem {
  METADATA_HEADER(AppMenuProxyActionItem, actions::ActionItem)
 public:
  explicit AppMenuProxyActionItem(actions::ActionItem* delegate);
  AppMenuProxyActionItem(const AppMenuProxyActionItem&) = delete;
  AppMenuProxyActionItem& operator=(const AppMenuProxyActionItem&) = delete;
  ~AppMenuProxyActionItem() override;

 private:
  void SyncWithDelegate();

  raw_ptr<actions::ActionItem> delegate_ = nullptr;

  // Required because the state of the delegate ActionItem can change at
  // runtime.
  base::CallbackListSubscription delegate_changed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_PROXY_ACTION_ITEM_H_
