// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/read_anything_immersive_overlay_view.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/read_anything/read_anything_immersive_web_view.h"
#include "chrome/browser/ui/view_ids.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

ReadAnythingImmersiveOverlayView::ReadAnythingImmersiveOverlayView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetID(VIEW_ID_READ_ANYTHING_OVERLAY);
  SetVisible(false);
}

ReadAnythingImmersiveOverlayView::~ReadAnythingImmersiveOverlayView() = default;

void ReadAnythingImmersiveOverlayView::ShowUI(
    std::unique_ptr<WebUIContentsWrapperT<ReadAnythingUntrustedUI>>
        contents_wrapper,
    ReadAnythingOpenTrigger trigger) {
  CHECK(!immersive_web_view_);
  auto immersive_web_view = std::make_unique<ReadAnythingImmersiveWebView>(
      std::move(contents_wrapper), trigger);
  immersive_web_view_ = AddChildView(std::move(immersive_web_view));
  SetVisible(true);
}

BEGIN_METADATA(ReadAnythingImmersiveOverlayView)
END_METADATA
