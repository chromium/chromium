// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PAINT_PREVIEW_COMPOSITOR_PAINT_PREVIEW_COMPOSITOR_IMPL_H_
#define COMPONENTS_SERVICES_PAINT_PREVIEW_COMPOSITOR_PAINT_PREVIEW_COMPOSITOR_IMPL_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/services/paint_preview_compositor/paint_preview_frame.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

class PaintPreviewCompositorImpl : public mojom::PaintPreviewCompositor {
 public:
  using FileMap = base::flat_map<uint64_t, base::File>;

  // Creates a new PaintPreviewCompositorImpl that receives mojo requests over
  // |receiver|. |receiver| should be created by the remote and
  // |disconnect_handler| is invoked when the remote closes the connection
  // invalidating |receiver|.
  //
  // For testing |receiver| can be a NullReceiver (i.e. a 'local' instance not
  // connected to a remote) and |disconnect_handler| should be a no-op.
  explicit PaintPreviewCompositorImpl(
      mojo::PendingReceiver<mojom::PaintPreviewCompositor> receiver,
      base::OnceClosure disconnect_handler);
  ~PaintPreviewCompositorImpl() override;

  // PaintPreviewCompositor implementation.
  void BeginComposite(mojom::PaintPreviewBeginCompositeRequestPtr request,
                      BeginCompositeCallback callback) override;
  void BitmapForFrame(uint64_t frame_guid,
                      const gfx::Rect& clip_rect,
                      float scale_factor,
                      BitmapForFrameCallback callback) override;
  void SetRootFrameUrl(const GURL& url) override;

 private:
  // Deserializes the contents of |file_handle| and associates it with the
  // metadata in |frame_proto|.
  PaintPreviewFrame DeserializeFrame(const PaintPreviewFrameProto& frame_proto,
                                     base::File file_handle);

  // Adds |frame_proto| to |frames_| and copies required data into |response|.
  // Consumes the corresponding file in |file_map|. Returns true on success.
  bool AddFrame(const PaintPreviewFrameProto& frame_proto,
                FileMap* file_map,
                mojom::PaintPreviewBeginCompositeResponsePtr* response);

  mojo::Receiver<mojom::PaintPreviewCompositor> receiver_{this};

  GURL url_;
  // A mapping from frame GUID to its associated data.
  base::flat_map<int64_t, PaintPreviewFrame> frames_;

  PaintPreviewCompositorImpl(const PaintPreviewCompositorImpl&) = delete;
  PaintPreviewCompositorImpl& operator=(const PaintPreviewCompositorImpl&) =
      delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_SERVICES_PAINT_PREVIEW_COMPOSITOR_PAINT_PREVIEW_COMPOSITOR_IMPL_H_
