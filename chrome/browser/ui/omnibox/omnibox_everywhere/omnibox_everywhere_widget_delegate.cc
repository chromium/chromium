// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"

#include "ui/base/hit_test.h"

namespace omnibox_everywhere {

OmniboxEverywhereWidgetDelegate::OmniboxEverywhereWidgetDelegate() {
  SetCanActivate(true);
  SetHasWindowSizeControls(false);
}

OmniboxEverywhereWidgetDelegate::~OmniboxEverywhereWidgetDelegate() = default;

void OmniboxEverywhereWidgetDelegate::SetDraggableRegion(
    std::optional<SkRegion> region) {
  draggable_region_ = std::move(region);
}

bool OmniboxEverywhereWidgetDelegate::IsPointInDraggableRegion(
    const gfx::Point& point) const {
  return draggable_region_ && !draggable_region_->isEmpty() &&
         draggable_region_->contains(point.x(), point.y());
}

int OmniboxEverywhereWidgetDelegate::NonClientHitTest(
    const gfx::Point& point) const {
  if (IsPointInDraggableRegion(point)) {
    return HTCAPTION;
  }
  return HTNOWHERE;
}

bool OmniboxEverywhereWidgetDelegate::ShouldDescendIntoChildForEventHandling(
    gfx::NativeView child,
    const gfx::Point& location) {
  return !IsPointInDraggableRegion(location);
}

}  // namespace omnibox_everywhere
