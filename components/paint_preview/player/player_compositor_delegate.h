// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_
#define COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_

#include "base/callback_forward.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace gfx {
class Rect;
}

class SkBitmap;

namespace paint_preview {

class PlayerCompositorDelegate {
 public:
  PlayerCompositorDelegate(const GURL& url);

  virtual void OnCompositorReady(
      const mojom::PaintPreviewBeginCompositeResponse& composite_response) = 0;

  // Called when there is a request for a new bitmap. When the bitmap
  // is ready, it will be passed to callback.
  void RequestBitmap(
      uint64_t frame_guid,
      const gfx::Rect& clip_rect,
      float scale_factor,
      base::OnceCallback<void(mojom::PaintPreviewCompositor::Status,
                              const SkBitmap&)> callback);

  // Called on touch event on a frame.
  void OnClick(uint64_t frame_guid, int x, int y);

 protected:
  virtual ~PlayerCompositorDelegate();

 private:
  // The current instance of PaintPreviewCompositor.
  mojo::Remote<mojom::PaintPreviewCompositor> paint_preview_compositor_;

  PlayerCompositorDelegate(const PlayerCompositorDelegate&) = delete;
  PlayerCompositorDelegate& operator=(const PlayerCompositorDelegate&) = delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_PLAYER_COMPOSITOR_DELEGATE_H_
