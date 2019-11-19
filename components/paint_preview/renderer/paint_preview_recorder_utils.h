// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_RENDERER_PAINT_PREVIEW_RECORDER_UTILS_H_
#define COMPONENTS_PAINT_PREVIEW_RENDERER_PAINT_PREVIEW_RECORDER_UTILS_H_

#include "base/files/file.h"
#include "cc/paint/paint_record.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-forward.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"

// These utilities are used by the PaintPreviewRecorderImpl. They are separate
// for testing purposes and to enforce restrictions caused by the lifetime of
// PaintPreviewServiceImpl being tied to it's associated RenderFrame.

namespace paint_preview {

class PaintPreviewTracker;

// Walks |buffer| to extract all the glyphs from its text blobs and writes
// them to |tracker|.
void ParseGlyphs(const cc::PaintOpBuffer* buffer, PaintPreviewTracker* tracker);

// Serializes |record| to |file| as an SkPicture of size |dimensions|. |tracker|
// supplies metadata required during serialization.
bool SerializeAsSkPicture(sk_sp<const cc::PaintRecord> record,
                          PaintPreviewTracker* tracker,
                          const gfx::Rect& dimensions,
                          base::File file);

// Builds a mojom::PaintPreviewCaptureResponse |response| using the data
// contained in |tracker|. Returns true on success.
// NOTE: |tracker| is effectively const here despite being passed by pointer.
void BuildResponse(PaintPreviewTracker* tracker,
                   mojom::PaintPreviewCaptureResponse* response);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_RENDERER_PAINT_PREVIEW_RECORDER_UTILS_H_
