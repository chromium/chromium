// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/main_background_region_view.h"

#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"

MainBackgroundRegionView::MainBackgroundRegionView(BrowserView& browser_view)
    : browser_view_(browser_view) {}
MainBackgroundRegionView::~MainBackgroundRegionView() = default;

void MainBackgroundRegionView::OnPaint(gfx::Canvas* canvas) {
  TopContainerBackground::PaintBackground(canvas, this, &browser_view_.get());
}

BEGIN_METADATA(MainBackgroundRegionView) END_METADATA
