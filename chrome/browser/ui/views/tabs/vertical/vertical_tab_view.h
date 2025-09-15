// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_

#include "base/callback_list.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class TabCollectionNode;

// View for a vertical tabstrip's tab.
class VerticalTabView : public views::View, public views::LayoutDelegate {
  METADATA_HEADER(VerticalTabView, views::View)

 public:
  explicit VerticalTabView(TabCollectionNode* collection_node);
  VerticalTabView(const VerticalTabView&) = delete;
  VerticalTabView& operator=(const VerticalTabView&) = delete;
  ~VerticalTabView() override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

 private:
  void ResetCollectionNode();

  raw_ptr<TabCollectionNode> collection_node_ = nullptr;

  base::CallbackListSubscription node_destroyed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_
