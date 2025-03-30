// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_SPLIT_TAB_DATA_H_
#define CHROME_BROWSER_UI_TABS_SPLIT_TAB_DATA_H_

#include "chrome/browser/ui/tabs/split_tab_collection.h"
#include "chrome/browser/ui/tabs/split_tab_id.h"
#include "components/tab_collections/public/tab_interface.h"
#include "ui/gfx/range/range.h"

class TabStripModel;

namespace split_tabs {

class SplitTabData {
 public:
  // TODO(crbug.com/392951786): Replace TabStripModel with SplitTabCollection.
  SplitTabData(TabStripModel* controller,
               const split_tabs::SplitTabId& id,
               tabs::SplitTabLayout split_layout);
  ~SplitTabData();

  const split_tabs::SplitTabId& id() const { return id_; }

  void set_split_layout(tabs::SplitTabLayout split_layout) {
    split_layout_ = split_layout;
  }
  tabs::SplitTabLayout split_layout() { return split_layout_; }

  std::vector<tabs::TabInterface*> ListTabs() const;

 private:
  raw_ptr<TabStripModel> controller_;
  tabs::SplitTabLayout split_layout_;
  split_tabs::SplitTabId id_;
};

}  // namespace split_tabs

#endif  // CHROME_BROWSER_UI_TABS_SPLIT_TAB_DATA_H_
