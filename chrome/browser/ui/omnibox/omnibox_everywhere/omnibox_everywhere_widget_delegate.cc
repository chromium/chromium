// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_widget_delegate.h"

#include "build/branding_buildflags.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/hit_test.h"
#include "ui/base/models/image_model.h"

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

ui::ImageModel OmniboxEverywhereWidgetDelegate::GetWindowIcon() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return ui::ImageModel::FromVectorIcon(vector_icons::kGoogleGLogoIcon);
#else
  return ui::ImageModel::FromVectorIcon(vector_icons::kSearchIcon);
#endif
}

ui::ImageModel OmniboxEverywhereWidgetDelegate::GetWindowAppIcon() {
  return GetWindowIcon();
}

}  // namespace omnibox_everywhere
