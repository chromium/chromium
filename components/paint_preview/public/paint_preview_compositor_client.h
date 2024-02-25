// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PUBLIC_PAINT_PREVIEW_COMPOSITOR_CLIENT_H_
#define COMPONENTS_PAINT_PREVIEW_PUBLIC_PAINT_PREVIEW_COMPOSITOR_CLIENT_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/unguessable_token.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "url/gurl.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace paint_preview {

// An instance of a paint preview compositor that is running in a utility
// process service. The class' lifetime is tied to that of the compositor
// running in the utility process (unless there is some kind of IPC disconnect
// that occurs).
class PaintPreviewCompositorClient {
 public:
  virtual ~PaintPreviewCompositorClient() = default;

  // Returns the token associated with the client. Will be null if the client
  // isn't started.
  virtual const std::optional<base::UnguessableToken>& Token() const = 0;

  // Adds `closure` as a disconnect handler.
  virtual void SetDisconnectHandler(base::OnceClosure closure) = 0;

  // Note the BitmapFor* methods use `clip_rect` values relative to the captured
  // content.

  // mojom::PaintPreviewCompositor API
  virtual void BeginSeparatedFrameComposite(
      mojom::PaintPreviewBeginCompositeRequestPtr request,
      mojom::PaintPreviewCompositor::BeginSeparatedFrameCompositeCallback
          callback) = 0;
  virtual void BitmapForSeparatedFrame(
      const base::UnguessableToken& frame_guid,
      const gfx::Rect& clip_rect,
      float scale_factor,
      mojom::PaintPreviewCompositor::BitmapForSeparatedFrameCallback callback,
      bool run_callback_on_default_task_runner = true) = 0;
  virtual void BeginMainFrameComposite(
      mojom::PaintPreviewBeginCompositeRequestPtr request,
      mojom::PaintPreviewCompositor::BeginMainFrameCompositeCallback
          callback) = 0;
  virtual void BitmapForMainFrame(
      const gfx::Rect& clip_rect,
      float scale_factor,
      mojom::PaintPreviewCompositor::BitmapForMainFrameCallback callback,
      bool run_callback_on_default_task_runner = true) = 0;
  virtual void SetRootFrameUrl(const GURL& url) = 0;

  PaintPreviewCompositorClient(const PaintPreviewCompositorClient&) = delete;
  PaintPreviewCompositorClient& operator=(const PaintPreviewCompositorClient&) =
      delete;

 protected:
  PaintPreviewCompositorClient() = default;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PUBLIC_PAINT_PREVIEW_COMPOSITOR_CLIENT_H_
