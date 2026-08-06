// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_DYNAMIC_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_DYNAMIC_HELPER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "ui/actions/actions.h"
#include "ui/base/window_open_disposition.h"

// This class is meant to support the dynamic content needed for the new app
// menu such as recent tabs, bookmarks and saved tab groups.

class AppMenuDynamicHelper {
 public:
  explicit AppMenuDynamicHelper(BrowserWindowInterface* browser);
  ~AppMenuDynamicHelper();

  void GetRecentTabsActions(actions::ActionItem* parent_item);

 private:
  void CreateRecentTabsAction(actions::BaseAction* parent_item,
                              const std::vector<RecentTabItem>& recent_items);

  void ExecuteRecentTab(const RecentTabItem& recent_item,
                        WindowOpenDisposition disposition,
                        actions::ActionItem* item,
                        actions::ActionInvocationContext context);

  void ExecuteRestoreEntry(SessionID id, WindowOpenDisposition disposition);

  void ExecuteRecentSplit(const RecentTabItem& recent_item,
                          WindowOpenDisposition disposition,
                          actions::ActionItem* item,
                          actions::ActionInvocationContext context);

  void BindCallbackToBuilder(actions::ActionItem::ActionItemBuilder& builder,
                             RecentTabItem recent_item,
                             WindowOpenDisposition disposition);

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  std::vector<RecentTabItem> recent_tabs_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_APP_MENU_DYNAMIC_HELPER_H_
