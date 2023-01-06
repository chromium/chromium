// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_RENDERER_PAINT_PREVIEW_RECORDER_UTILS_H_
#define COMPONENTS_PAINT_PREVIEW_RENDERER_PAINT_PREVIEW_RECORDER_UTILS_H_

#include "base/files/file.h"
#include "cc/paint/paint_record.h"
#include "components/paint_preview/common/mojom/paint_preview_recorder.mojom-forward.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/geometry/rect.h"

// These utilities are used by the PaintPreviewRecorderImpl. They are separate
// for testing purposes and to enforce restrictions caused by the lifetime of
// PaintPreviewServiceImpl being tied to it's associated RenderFrame.

namespace paint_preview {

class PaintPreviewTracker;

// Converts `recording` into an SkPicture, tracking embedded content. During
// conversion:
// 1. Walks |buffer| to extract all the glyphs from its text blobs and links.
//    The extracted data is written to `tracker`.
// 2. Tracks geometry changes for frames and saves them to `tracker`.
// 3. Unaccelerates GPU accelerated PaintImages.
// Returns `nullptr` if the resulting picture failed or zero sized.
sk_sp<const SkPicture> PaintRecordToSkPicture(const cc::PaintRecord& recording,
                                              PaintPreviewTracker* tracker,
                                              const gfx::Rect& bounds);

// NOTE: |tracker| is effectively const here despite being passed by pointer.
void BuildResponse(PaintPreviewTracker* tracker,
                   mojom::PaintPreviewCaptureResponse* response);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_RENDERER_PAINT_PREVIEW_RECORDER_UTILS_H_
