// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTAINER_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTAINER_LAYOUT_H_

#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/layout_manager_base.h"

enum class PinnedToolbarActionFlexPriority {
  // Pinned toolbar action that should be visible in the layout.
  kHigh = 0,
  // Pinned toolbar action that should have the highest priority if there is
  // extra space in the layout.
  kMedium = 1,
  // Pinned toolbar action that has the lowest priority of being included if
  // there is extra space in the layout.
  kLow = 2,
};

class PinnedToolbarActionsContainerLayout : public views::LayoutManagerBase {
 public:
  PinnedToolbarActionsContainerLayout() = default;
  PinnedToolbarActionsContainerLayout(
      const PinnedToolbarActionsContainerLayout&) = delete;
  PinnedToolbarActionsContainerLayout& operator=(
      const PinnedToolbarActionsContainerLayout&) = delete;
  ~PinnedToolbarActionsContainerLayout() override = default;

  // LayoutManagerBase:
  views::ProposedLayout CalculateProposedLayout(
      const views::SizeBounds& size_bounds) const override;

  void SetInteriorMargin(const gfx::Insets& interior_margin);
  const gfx::Insets& interior_margin() const { return interior_margin_; }

 private:
  // Spacing between child views and host view border.
  gfx::Insets interior_margin_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_ACTIONS_CONTAINER_LAYOUT_H_
