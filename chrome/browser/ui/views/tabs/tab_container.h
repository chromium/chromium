// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_

#include <memory>
#include "chrome/browser/ui/views/tabs/tab.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class TabStrip;
class TabGroupHeader;

// A View that contains a sequence of Tabs for the TabStrip.
class TabContainer : public views::View {
 public:
  METADATA_HEADER(TabContainer);

  explicit TabContainer(TabStrip* tab_strip) : tab_strip_(tab_strip) {}
  ~TabContainer() override;

  Tab* AddTab(std::unique_ptr<Tab> tab, int model_index);
  void MoveTab(Tab* tab, int from_model_index, int to_model_index);

  void MoveGroupHeader(TabGroupHeader* group_header, int first_tab_model_index);

 private:
  // Returns the corresponding view index of a |tab| to be inserted at
  // |to_model_index|. Used to reorder the child views of the tab container
  // so that focus order stays consistent with the visual tab order.
  // |from_model_index| is where the tab currently is, if it's being moved
  // instead of added.
  int GetViewInsertionIndex(absl::optional<tab_groups::TabGroupId> group,
                            absl::optional<int> from_model_index,
                            int to_model_index) const;

  // Hopefully temporary pointer to containing TabStrip, needed until more
  // members such as |tabs_| are moved down into TabContainer.
  TabStrip* tab_strip_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTAINER_H_
