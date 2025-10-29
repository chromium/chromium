// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/read_anything/immersive_read_anything_overlay_view.h"

#include <memory>

#include "chrome/browser/ui/view_ids.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"

ImmersiveReadAnythingOverlayView::ImmersiveReadAnythingOverlayView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetID(VIEW_ID_READ_ANYTHING_OVERLAY);
  SetVisible(false);
}

ImmersiveReadAnythingOverlayView::~ImmersiveReadAnythingOverlayView() = default;

BEGIN_METADATA(ImmersiveReadAnythingOverlayView)
END_METADATA
