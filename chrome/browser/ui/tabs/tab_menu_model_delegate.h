// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_DELEGATE_H_

#include <vector>

#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

class BrowserWindowInterface;

///////////////////////////////////////////////////////////////////////////////
//
// TabMenuModelDelegate
//
//  A delegate interface that the TabMenuModel uses to perform work that it
//  can't do itself, such as retrieving the list of existing browsers that tabs
//  can be moved to.
//
//  This interface is typically implemented by the controller that instantiates
//  the TabMenuModel (in our case the Browser object).
//
///////////////////////////////////////////////////////////////////////////////
class TabMenuModelDelegate {
 public:
  DECLARE_USER_DATA(TabMenuModelDelegate);

  // `host` is the UnownedUserDataHost of the browser window this delegate
  // serves.
  explicit TabMenuModelDelegate(ui::UnownedUserDataHost& host);
  virtual ~TabMenuModelDelegate();

  // Returns the delegate for `browser`, or null if it does not have one.
  static TabMenuModelDelegate* From(BrowserWindowInterface* browser);

  // Returns a list of other existing browser windows that can accept menu
  // operations (i.e. Move tab to new window, Add tab to group) that are not the
  // current browser this was called on.
  virtual std::vector<BrowserWindowInterface*> GetOtherBrowserWindows(
      bool is_app) = 0;

  virtual tab_groups::TabGroupSyncService* GetTabGroupSyncService() = 0;

 private:
  ui::ScopedUnownedUserData<TabMenuModelDelegate> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_DELEGATE_H_
