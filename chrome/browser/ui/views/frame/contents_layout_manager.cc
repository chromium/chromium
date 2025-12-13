// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_layout_manager.h"

#include "base/check.h"
#include "ui/views/view.h"

ContentsLayoutManager::ContentsLayoutManager(views::View* contents_view,
                                             views::View* lens_overlay_view)
    : contents_view_(contents_view), lens_overlay_view_(lens_overlay_view) {}

ContentsLayoutManager::~ContentsLayoutManager() = default;

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
  gfx::Rect contents_bounds(0, 0, container_size.width(),
                            container_size.height());

  const auto& contents_rect = host_view()->GetMirroredRect(contents_bounds);
  views::SizeBounds optional_size_bound = views::SizeBounds(container_size);
  layouts.child_layouts.emplace_back(contents_view_.get(),
                                     contents_view_->GetVisible(),
                                     contents_bounds, optional_size_bound);

  // The Lens overlay view bounds are the same as the contents view.
  CHECK(lens_overlay_view_);
  layouts.child_layouts.emplace_back(lens_overlay_view_.get(),
                                     lens_overlay_view_->GetVisible(),
                                     contents_rect, optional_size_bound);

  layouts.host_size = gfx::Size(width, height);
  return layouts;
}
