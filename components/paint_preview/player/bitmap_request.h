// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PLAYER_BITMAP_REQUEST_H_
#define COMPONENTS_PAINT_PREVIEW_PLAYER_BITMAP_REQUEST_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/unguessable_token.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

struct BitmapRequest {
  using BitmapRequestCallback =
      base::OnceCallback<void(mojom::PaintPreviewCompositor::BitmapStatus,
                              const SkBitmap&)>;

  BitmapRequest(const std::optional<base::UnguessableToken>& frame_guid,
                const gfx::Rect& clip_rect,
                float scale_factor,
                BitmapRequestCallback callback,
                bool run_callback_on_default_task_runner);
  ~BitmapRequest();

  BitmapRequest& operator=(BitmapRequest&& other) noexcept;
  BitmapRequest(BitmapRequest&& other) noexcept;

  std::optional<base::UnguessableToken> frame_guid;
  gfx::Rect clip_rect;
  float scale_factor;
  BitmapRequestCallback callback;
  bool run_callback_on_default_task_runner;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_BITMAP_REQUEST_H_
