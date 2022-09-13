// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_RENDERER_PAINT_PREVIEW_RECORDER_IMPL_H_
#define COMPONENTS_PAINT_PREVIEW_RENDERER_PAINT_PREVIEW_RECORDER_IMPL_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace paint_preview {

// PaintPreviewRecorderImpl handles the majority of the grunt work for capturing
// a paint preview of a RenderFrame.
class PaintPreviewRecorderImpl : public content::RenderFrameObserver,
                                 mojom::PaintPreviewRecorder {
 public:
  PaintPreviewRecorderImpl(content::RenderFrame* render_frame);
  ~PaintPreviewRecorderImpl() override;

  void CapturePaintPreview(mojom::PaintPreviewCaptureParamsPtr params,
                           CapturePaintPreviewCallback callback) override;

 private:
  // RenderFrameObserver implementation --------------------------------------

  void OnDestruct() override;

  // Helpers ------------------------------------------------------------------

  void BindPaintPreviewRecorder(
      mojo::PendingAssociatedReceiver<mojom::PaintPreviewRecorder> receiver);

  // Handles the bulk of the capture.
  void CapturePaintPreviewInternal(
      const mojom::PaintPreviewCaptureParamsPtr& params,
      mojom::PaintPreviewCaptureResponsePtr region,
      CapturePaintPreviewCallback callback);

  bool is_painting_preview_;
  const bool is_main_frame_;
  mojo::AssociatedReceiver<mojom::PaintPreviewRecorder>
      paint_preview_recorder_receiver_{this};

  base::WeakPtrFactory<PaintPreviewRecorderImpl> weak_ptr_factory_{this};

  PaintPreviewRecorderImpl(const PaintPreviewRecorderImpl&) = delete;
  PaintPreviewRecorderImpl& operator=(const PaintPreviewRecorderImpl&) = delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_RENDERER_PAINT_PREVIEW_RECORDER_IMPL_H_
