// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/main_background_region_view.h"

#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "ui/base/metadata/metadata_impl_macros.h"

MainBackgroundRegionView::MainBackgroundRegionView(BrowserView& browser_view) {
  SetCanProcessEventsWithinSubtree(false);
  SetVisible(false);
  const auto primary_color =
      base::FeatureList::IsEnabled(features::kDetachedTabs)
          ? CustomCornersBackground::ColorChoice(
                CustomCornersBackground::FrameTheme())
          : CustomCornersBackground::ColorChoice(
                CustomCornersBackground::ToolbarTheme());
  SetBackground(std::make_unique<CustomCornersBackground>(
      *this, browser_view, primary_color,
      CustomCornersBackground::FrameTheme()));
}

MainBackgroundRegionView::~MainBackgroundRegionView() = default;

BEGIN_METADATA(MainBackgroundRegionView) END_METADATA
