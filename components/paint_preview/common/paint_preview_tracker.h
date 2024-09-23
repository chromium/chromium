// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_PAINT_PREVIEW_TRACKER_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_PAINT_PREVIEW_TRACKER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/common/glyph_usage.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom.h"
#include "components/paint_preview/common/serial_utils.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace paint_preview {

// Tracks metadata for a Paint Preview. Contains all the data required to
// produce a PaintPreviewFrameProto.
class PaintPreviewTracker {
 public:
  PaintPreviewTracker(
      const base::UnguessableToken& guid,
      const std::optional<base::UnguessableToken>& embedding_token,
      bool is_main_frame);
  ~PaintPreviewTracker();

  // Getters ------------------------------------------------------------------

  const base::UnguessableToken& Guid() const { return guid_; }
  const std::optional<base::UnguessableToken>& EmbeddingToken() const {
    return embedding_token_;
  }
  bool IsMainFrame() const { return is_main_frame_; }

  // Transform methods --------------------------------------------------------

  // Used to transform the position of links when parsing the paint op buffer.
  // These are inspired by the methods in SkCanvas.
  void Save();
  void Restore();
  void SetMatrix(const SkMatrix& matrix);
  void Concat(const SkMatrix& matrix);
  void Scale(SkScalar x, SkScalar y);
  void Rotate(SkScalar degrees);
  void Translate(SkScalar x, SkScalar y);

  // Data Collection ----------------------------------------------------------

  // Creates a placeholder SkPicture for an OOP subframe located at |rect|
  // mapped to the |embedding_token| of OOP RenderFrame. Returns the content id
  // of the placeholder SkPicture.
  uint32_t CreateContentForRemoteFrame(
      const gfx::Rect& rect,
      const base::UnguessableToken& embedding_token);

  // Adds the glyphs in |blob| to the glyph usage tracker for the |blob|'s
  // associated typface.
  void AddGlyphs(const SkTextBlob* blob);

  // Adds |link| with bounding box |rect| to the list of links.
  void AnnotateLink(const GURL& link, const SkRect& rect);

  // Used when walking the PaintOpBuffer to transforms the clip rect associated
  // with the SkPicture with |id| to be relative to the top-left corner of
  // the root space of the current frame associated with this tracker using
  // |matrix_|. Originally the clip rect is relative to the current matrix stack
  // of the canvas at time of drawing. The result in stored and accessible via
  // the PictureSerializationContext during serialization.
  void TransformClipForFrame(uint32_t id);

  // Data Serialization -------------------------------------------------------
  // NOTE: once any of these methods are called the PaintPreviewTracker should
  // be considered immutable.

  // Inserts the OOP subframe placeholder associated with |content_id| into
  // |canvas|. The cull rect of the placeholder will encode the position and
  // size of the the subframe in its parent's coordinate system.
  void CustomDataToSkPictureCallback(SkCanvas* canvas, uint32_t content_id);

  // Expose internal maps for use in MakeSerialProcs().
  // NOTE: Cannot be const due to how SkPicture procs work.
  PictureSerializationContext* GetPictureSerializationContext() {
    return &picture_context_;
  }
  TypefaceUsageMap* GetTypefaceUsageMap() { return &typeface_glyph_usage_; }
  ImageSerializationContext* GetImageSerializationContext() {
    return &image_context_;
  }

  // Expose links for serialization to a PaintPreviewFrameProto.
  const std::vector<mojom::LinkDataPtr>& GetLinks() { return links_; }

  // Moves |links_| to out. Invalidates existing entries in |links_|.
  void MoveLinks(std::vector<mojom::LinkDataPtr>* out);

  const base::flat_map<uint32_t, sk_sp<SkPicture>>&
  GetSubframePicsForTesting() {
    return subframe_pics_;
  }

 private:
  const base::UnguessableToken guid_;
  const std::optional<base::UnguessableToken> embedding_token_;
  const bool is_main_frame_;

  // TODO(crbug.com/40735471): Change this to an SkM44.
  SkMatrix matrix_;
  std::vector<SkMatrix> states_;

  std::vector<mojom::LinkDataPtr> links_;
  ImageSerializationContext image_context_;
  PictureSerializationContext picture_context_;
  TypefaceUsageMap typeface_glyph_usage_;
  base::flat_map<uint32_t, sk_sp<SkPicture>> subframe_pics_;

  PaintPreviewTracker(const PaintPreviewTracker&) = delete;
  PaintPreviewTracker& operator=(const PaintPreviewTracker&) = delete;
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_PAINT_PREVIEW_TRACKER_H_
