// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_SPLIT_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_SPLIT_TAB_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

class TabCollectionNode;
class GlowHoverController;

// Container for the vertical tabstrip's split tabs.
class VerticalSplitTabView : public views::View, public views::LayoutDelegate {
  METADATA_HEADER(VerticalSplitTabView, views::View)

 public:
  explicit VerticalSplitTabView(TabCollectionNode* collection_node);
  VerticalSplitTabView(const VerticalSplitTabView&) = delete;
  VerticalSplitTabView& operator=(const VerticalSplitTabView&) = delete;
  ~VerticalSplitTabView() override;

  // views::View
  void OnThemeChanged() override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  // Used by children to sync their animation values with the parent.
  double GetHoverAnimationValue() const;
  GlowHoverController* hover_controller() const {
    return hover_controller_.get();
  }

  const TabCollectionNode* collection_node() const { return collection_node_; }

 private:
  void ResetCollectionNode();
  void OnDataChanged();
  void UpdateBorder();
  void UpdateHovered(bool hovered);

  raw_ptr<TabCollectionNode> collection_node_ = nullptr;
  bool hovered_ = false;
  bool pinned_ = false;
  std::unique_ptr<GlowHoverController> hover_controller_;

  base::CallbackListSubscription data_changed_subscription_;
  base::CallbackListSubscription node_destroyed_subscription_;
  base::CallbackListSubscription paint_as_active_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_SPLIT_TAB_VIEW_H_
