// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_animation_content_view.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/layout/fill_layout.h"

SidePanelAnimationContentView::SidePanelAnimationContentView() {
  SetUseDefaultFillLayout(true);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

SidePanelAnimationContentView::SidePanelAnimationContentView(
    views::View* content)
    : SidePanelAnimationContentView() {
  AddChildView(content);
}

SidePanelAnimationContentView::~SidePanelAnimationContentView() = default;

void SidePanelAnimationContentView::UpdateHorizontalTranslation(
    double translation_x) {
  gfx::Transform horizontal_translation;
  horizontal_translation.Translate(translation_x, 0);
  layer()->SetTransform(horizontal_translation);
}

void SidePanelAnimationContentView::ClipBounds(const gfx::Rect& bounds) {
  layer()->SetClipRect(bounds);
}

BEGIN_METADATA(SidePanelAnimationContentView)
END_METADATA
