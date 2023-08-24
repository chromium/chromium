// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_DELEGATE_H_

#include <vector>

class Browser;

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
  virtual ~TabMenuModelDelegate() {}

  // Returns a list of other existing browser windows that can accept menu
  // operations (i.e. Move tab to new window, Add tab to group) that are not the
  // current browser this was called on.
  virtual std::vector<Browser*> GetOtherBrowserWindows(bool is_app) = 0;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_MENU_MODEL_DELEGATE_H_
