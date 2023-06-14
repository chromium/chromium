// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_header.h"

#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/views/layout/flex_layout.h"

SidePanelHeader::SidePanelHeader() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

void SidePanelHeader::Layout() {
  views::View::Layout();

  if (features::IsChromeRefresh2023()) {
    // The side panel header should draw on top of its parent's border.
    gfx::Rect contents_bounds = parent()->GetContentsBounds();

    constexpr int kHeaderPaddingBottom = 6;
    gfx::Rect header_bounds =
        gfx::Rect(contents_bounds.x(),
                  contents_bounds.y() - GetPreferredSize().height() -
                      kHeaderPaddingBottom,
                  contents_bounds.width(), GetPreferredSize().height());

    SetBoundsRect(header_bounds);
  }
}
