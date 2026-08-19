// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_RECENT_TABS_DYNAMIC_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_RECENT_TABS_DYNAMIC_MENU_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/recent_tabs_builder.h"
#include "chrome/browser/ui/tabs/recent_tabs_sub_menu_model.h"
#include "components/favicon_base/favicon_types.h"
#include "ui/actions/actions.h"
#include "ui/base/window_open_disposition.h"

// This class is meant to support the dynamic content needed for the recent tabs
// submenu
class RecentTabsDynamicMenu {
 public:
  explicit RecentTabsDynamicMenu(BrowserWindowInterface* browser);
  ~RecentTabsDynamicMenu();

  base::WeakPtr<RecentTabsDynamicMenu> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void BuildRecentTabsActions(actions::BaseAction* parent_item);

 private:
  void CreateRecentTabsAction(actions::BaseAction* parent_item,
                              const std::vector<RecentTabItem>& recent_items);

  void FetchFavicon(actions::ActionItem* action_item,
                    const RecentTabItem& item);
  void OnFaviconDataAvailable(
      base::WeakPtr<actions::ActionItem> action_item,
      const favicon_base::FaviconImageResult& image_result);

  void ExecuteRecentTab(const RecentTabItem& recent_item,
                        WindowOpenDisposition disposition,
                        actions::ActionItem* item,
                        actions::ActionInvocationContext context);

  void ExecuteRestoreEntry(SessionID id, WindowOpenDisposition disposition);

  void ExecuteRecentSplit(const RecentTabItem& recent_item,
                          WindowOpenDisposition disposition,
                          actions::ActionItem* item,
                          actions::ActionInvocationContext context);

  actions::ActionItem::InvokeActionCallback GetInvokeCallback(
      RecentTabItem recent_item,
      WindowOpenDisposition disposition);

  raw_ptr<BrowserWindowInterface> browser_window_interface_;
  base::CancelableTaskTracker cancelable_task_tracker_;
  base::WeakPtrFactory<RecentTabsDynamicMenu> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_RECENT_TABS_DYNAMIC_MENU_H_
