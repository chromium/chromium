// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_UNPINNED_TAB_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_UNPINNED_TAB_CONTAINER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/view.h"

// Container for the vertical tabstrip's unpinned tabs.
class VerticalUnpinnedTabContainerView : public views::View,
                                         public views::LayoutDelegate {
  METADATA_HEADER(VerticalUnpinnedTabContainerView, views::View)

 public:
  VerticalUnpinnedTabContainerView();
  VerticalUnpinnedTabContainerView(const VerticalUnpinnedTabContainerView&) =
      delete;
  VerticalUnpinnedTabContainerView& operator=(
      const VerticalUnpinnedTabContainerView&) = delete;
  ~VerticalUnpinnedTabContainerView() override;

  // LayoutDelegate:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_UNPINNED_TAB_CONTAINER_VIEW_H_
