// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_view.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/layout/box_layout.h"

MediaView::MediaView(bool is_subsection) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const auto& insets =
      is_subsection ? provider->GetInsetsMetric(INSETS_PAGE_INFO_HOVER_BUTTON)
                    : gfx::Insets();
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  SetInsideBorderInsets(insets);
}

void MediaView::RefreshSize() {
  PreferredSizeChanged();
}

void MediaView::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

gfx::Size MediaView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return {
      width(),
      views::BoxLayoutView::CalculatePreferredSize(available_size).height()};
}

MediaView::~MediaView() = default;

BEGIN_METADATA(MediaView)
END_METADATA
