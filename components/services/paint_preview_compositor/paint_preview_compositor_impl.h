// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_PAINT_PREVIEW_COMPOSITOR_PAINT_PREVIEW_COMPOSITOR_IMPL_H_
#define COMPONENTS_SERVICES_PAINT_PREVIEW_COMPOSITOR_PAINT_PREVIEW_COMPOSITOR_IMPL_H_

#include <stdint.h>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "components/discardable_memory/client/client_discardable_shared_memory_manager.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/recording_map.h"
#include "components/services/paint_preview_compositor/paint_preview_frame.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

struct SkpResult;

class PaintPreviewCompositorImpl : public mojom::PaintPreviewCompositor {
 public:
  using FileMap = base::flat_map<base::UnguessableToken, base::File>;

  // Creates a new PaintPreviewCompositorImpl that receives mojo requests over
  // |receiver|. |receiver| should be created by the remote and
  // |disconnect_handler| is invoked when the remote closes the connection
  // invalidating |receiver|.
  //
  // For testing |receiver| can be a NullReceiver (i.e. a 'local' instance not
  // connected to a remote) and |disconnect_handler| should be a no-op.
  explicit PaintPreviewCompositorImpl(
      mojo::PendingReceiver<mojom::PaintPreviewCompositor> receiver,
      scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
          discardable_shared_memory_manager,
      base::OnceClosure disconnect_handler);
  ~PaintPreviewCompositorImpl() override;

  PaintPreviewCompositorImpl(const PaintPreviewCompositorImpl&) = delete;
  PaintPreviewCompositorImpl& operator=(const PaintPreviewCompositorImpl&) =
      delete;

  // PaintPreviewCompositor implementation.
  void BeginSeparatedFrameComposite(
      mojom::PaintPreviewBeginCompositeRequestPtr request,
      BeginSeparatedFrameCompositeCallback callback) override;
  void BitmapForSeparatedFrame(
      const base::UnguessableToken& frame_guid,
      const gfx::Rect& clip_rect,
      float scale_factor,
      BitmapForSeparatedFrameCallback callback) override;
  void BeginMainFrameComposite(
      mojom::PaintPreviewBeginCompositeRequestPtr request,
      BeginMainFrameCompositeCallback callback) override;
  void BitmapForMainFrame(const gfx::Rect& clip_rect,
                          float scale_factor,
                          BitmapForMainFrameCallback callback) override;
  void SetRootFrameUrl(const GURL& url) override;

 private:
  // Adds |frame_proto| to |frames_| and copies required data into |response|.
  // Consumes the corresponding file in |file_map|. Returns true on success.
  bool AddFrame(
      const PaintPreviewFrameProto& frame_proto,
      const base::flat_map<base::UnguessableToken, SkpResult>& skp_map,
      mojom::PaintPreviewBeginCompositeResponsePtr* response);

  static base::flat_map<base::UnguessableToken, SkpResult> DeserializeAllFrames(
      RecordingMap&& recording_map);

  // Deserialize a the recording of the frame specified by |frame_proto|.
  // Subframes are recursed into and loaded into |loaded_frames| so the current
  // frame will have them available during its own deserialization. Recordings
  // are erased from |recording_map| as they are consumed.
  // |subframe_failed| returns whether or not any subframes (or subframes of
  // subframes) failed during deserialization.
  // The resulting picture will contain subframes (or an empty placeholder in
  // the case of failed subframes) or will return |nullptr| on failure.
  static sk_sp<SkPicture> DeserializeFrameRecursive(
      const PaintPreviewFrameProto& frame_proto,
      const PaintPreviewProto& proto,
      base::flat_map<base::UnguessableToken, sk_sp<SkPicture>>* loaded_frames,
      RecordingMap* recording_map,
      bool* subframe_failed);

  mojo::Receiver<mojom::PaintPreviewCompositor> receiver_{this};

  GURL url_;

  // A mapping from frame GUID to its associated data. Empty until
  // |BeginSeparatedFrameComposite| is called.
  // Must be modified only by |BeginSeparatedFrameComposite|.
  base::flat_map<base::UnguessableToken, PaintPreviewFrame> frames_;

  // Contains the root frame, including content from subframes. |nullptr| until
  // |BeginMainFrameComposite| succeeds.
  // Must be modified only by |BeginMainFrameComposite|.
  sk_sp<SkPicture> root_frame_;

  scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
      discardable_shared_memory_manager_;

  base::WeakPtrFactory<PaintPreviewCompositorImpl> weak_ptr_factory_{this};
};

}  // namespace paint_preview

#endif  // COMPONENTS_SERVICES_PAINT_PREVIEW_COMPOSITOR_PAINT_PREVIEW_COMPOSITOR_IMPL_H_
