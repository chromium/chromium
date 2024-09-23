// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_TAB_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_TAB_LIST_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

class TabListModel;
class TabListRowView;

namespace resource_attribution {
class PageContext;
}

class TabListView : public views::View {
  METADATA_HEADER(TabListView, views::View)
 public:
  explicit TabListView(TabListModel* tab_list_model);
  ~TabListView() override;

  TabListView(const TabListView&) = delete;
  TabListView& operator=(const TabListView&) = delete;

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  void RemoveRow(resource_attribution::PageContext context,
                 TabListRowView* row_view);

  raw_ptr<TabListModel> tab_list_model_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERFORMANCE_CONTROLS_TAB_LIST_VIEW_H_
