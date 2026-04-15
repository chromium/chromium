// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_TAB_TITLE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_TAB_TITLE_H_

#include "ui/views/controls/label.h"

// The class that is the view for the tab title. This is shared
// across both horizontal and vertical tab strip.
class TabTitle : public views::Label {
  METADATA_HEADER(TabTitle, views::Label)
 public:
  TabTitle();
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_TAB_TITLE_H_
