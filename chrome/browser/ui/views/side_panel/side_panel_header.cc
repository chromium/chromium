// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_header.h"

#include "chrome/browser/ui/ui_features.h"
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

    const int header_padding_bottom =
        features::IsSidePanelPinningEnabled() ? 0 : 6;
    gfx::Rect header_bounds =
        gfx::Rect(contents_bounds.x(),
                  contents_bounds.y() - GetPreferredSize().height() -
                      header_padding_bottom,
                  contents_bounds.width(), GetPreferredSize().height());

    SetBoundsRect(header_bounds);
  }
}
