// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/player/player_compositor_delegate.h"

#include <vector>

#include "base/callback.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

PlayerCompositorDelegate::PlayerCompositorDelegate(const GURL& url) {
  // TODO(crbug.com/1019885): Use url to get proto and file map.
  // TODO(crbug.com/1019885): Initialize the PaintPreviewCompositor class.
  // TODO(crbug.com/1019883): Initialize the HitTester class.
}

void PlayerCompositorDelegate::RequestBitmap(
    uint64_t frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    base::OnceCallback<void(mojom::PaintPreviewCompositor::Status,
                            const SkBitmap&)> callback) {
  if (!paint_preview_compositor_ || !paint_preview_compositor_.is_bound()) {
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::Status::kCompositingFailure, SkBitmap());
    return;
  }

  paint_preview_compositor_->BitmapForFrame(frame_guid, clip_rect, scale_factor,
                                            std::move(callback));
}

void PlayerCompositorDelegate::OnClick(uint64_t frame_guid, int x, int y) {
  // TODO(crbug.com/1019883): Handle url clicks with the HitTester class.
}

PlayerCompositorDelegate::~PlayerCompositorDelegate() = default;

}  // namespace paint_preview
