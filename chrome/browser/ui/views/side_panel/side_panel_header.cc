// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_header.h"

#include "ui/compositor/layer.h"
#include "ui/views/layout/flex_layout.h"

SidePanelHeader::SidePanelHeader() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

void SidePanelHeader::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  // The side panel header should draw on top of its parent's border.
  gfx::Rect contents_bounds = parent()->GetContentsBounds();

  gfx::Rect header_bounds = gfx::Rect(
      contents_bounds.x(), contents_bounds.y() - GetPreferredSize().height(),
      contents_bounds.width(), GetPreferredSize().height());

  SetBoundsRect(header_bounds);
}
