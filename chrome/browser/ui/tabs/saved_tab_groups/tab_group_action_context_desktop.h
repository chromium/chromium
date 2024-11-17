// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_ACTION_CONTEXT_DESKTOP_H_
#define CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_ACTION_CONTEXT_DESKTOP_H_

#include "base/memory/raw_ptr.h"
#include "components/saved_tab_groups/public/types.h"

class Browser;

namespace tab_groups {

// Desktop implementation of TabGroupActionContext used to help with opening
// saved tab groups in the browser.
struct TabGroupActionContextDesktop : public TabGroupActionContext {
  TabGroupActionContextDesktop(Browser* browser, OpeningSource opening_source);
  ~TabGroupActionContextDesktop() override = default;

  raw_ptr<Browser> browser;
  OpeningSource opening_source;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_UI_TABS_SAVED_TAB_GROUPS_TAB_GROUP_ACTION_CONTEXT_DESKTOP_H_
