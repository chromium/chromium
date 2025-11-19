// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class TabCollectionNode;

namespace views {
class Label;
}

// Container for a tab group in the vertical tabstrip.
class VerticalTabGroupView : public views::View, public views::LayoutDelegate {
  METADATA_HEADER(VerticalTabGroupView, views::View)

 public:
  explicit VerticalTabGroupView(TabCollectionNode* collection_node);
  VerticalTabGroupView(const VerticalTabGroupView&) = delete;
  VerticalTabGroupView& operator=(const VerticalTabGroupView&) = delete;
  ~VerticalTabGroupView() override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

 private:
  void ResetCollectionNode();

  void OnDataChanged();

  raw_ptr<TabCollectionNode> collection_node_ = nullptr;

  base::CallbackListSubscription node_destroyed_subscription_;
  base::CallbackListSubscription data_changed_subscription_;

  const raw_ptr<views::Label> group_header_ = nullptr;
  const raw_ptr<views::View> group_line_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_GROUP_VIEW_H_
