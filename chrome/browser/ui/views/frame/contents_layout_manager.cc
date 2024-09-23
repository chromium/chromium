// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_layout_manager.h"

#include "ui/views/view.h"

ContentsLayoutManager::ContentsLayoutManager(views::View* devtools_view,
                                             views::View* contents_view,
                                             views::View* watermark_view)
    : devtools_view_(devtools_view),
      contents_view_(contents_view),
      watermark_view_(watermark_view) {}

ContentsLayoutManager::~ContentsLayoutManager() = default;

void ContentsLayoutManager::SetContentsResizingStrategy(
    const DevToolsContentsResizingStrategy& strategy) {
  if (strategy_.Equals(strategy))
    return;

  strategy_.CopyFrom(strategy);
  InvalidateHost(true);
}

views::ProposedLayout ContentsLayoutManager::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;

  // If the |size_bounds| isn't bounded, the preferred size is being requested.
  if (!size_bounds.is_fully_bounded()) {
    return layouts;
  }
  int height = size_bounds.height().value();
  int width = size_bounds.width().value();

  gfx::Size container_size(width, height);
  gfx::Rect new_devtools_bounds;
  gfx::Rect new_contents_bounds;

  ApplyDevToolsContentsResizingStrategy(strategy_, container_size,
      &new_devtools_bounds, &new_contents_bounds);

  // DevTools cares about the specific position, so we have to compensate RTL
  // layout here.
  layouts.child_layouts.emplace_back(
      devtools_view_.get(), devtools_view_->GetVisible(),
      host_view()->GetMirroredRect(new_devtools_bounds),
      views::SizeBounds(container_size));
  layouts.child_layouts.emplace_back(
      contents_view_.get(), contents_view_->GetVisible(),
      host_view()->GetMirroredRect(new_contents_bounds),
      views::SizeBounds(container_size));

  // Enterprise watermark view is always overlaid, even when empty.
  if (watermark_view_) {
    layouts.child_layouts.emplace_back(
        watermark_view_.get(), watermark_view_->GetVisible(),
        gfx::Rect(0, 0, width, height), views::SizeBounds(container_size));
  }
  layouts.host_size = gfx::Size(width, height);
  return layouts;
}
