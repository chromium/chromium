// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/layout/delegating_layout_manager.h"

class VerticalUnpinnedTabContainerView;

namespace views {
class Separator;
class View;
}  // namespace views

// Container that holds the pinned and unpinned tabs in the vertical tab strip.
class VerticalTabStripView final : public views::View,
                                   public views::LayoutDelegate {
  METADATA_HEADER(VerticalTabStripView, views::View)

 public:
  VerticalTabStripView();
  VerticalTabStripView(const VerticalTabStripView&) = delete;
  VerticalTabStripView& operator=(const VerticalTabStripView&) = delete;
  ~VerticalTabStripView() override;

  views::Separator* tabs_separator_for_testing() { return tabs_separator_; }
  views::View* pinned_tabs_container_for_testing() {
    return pinned_tabs_container_;
  }
  VerticalUnpinnedTabContainerView* unpinned_tabs_container_for_testing() {
    return unpinned_tabs_container_;
  }

  void SetCollapsedState(bool is_collapsed);

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

 private:
  raw_ptr<views::View> pinned_tabs_container_ = nullptr;
  raw_ptr<views::Separator> tabs_separator_ = nullptr;
  raw_ptr<VerticalUnpinnedTabContainerView> unpinned_tabs_container_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_STRIP_VIEW_H_
