// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/paint_preview_compositor/paint_preview_frame.h"

namespace paint_preview {

PaintPreviewFrame::PaintPreviewFrame() = default;
PaintPreviewFrame::~PaintPreviewFrame() = default;

PaintPreviewFrame::PaintPreviewFrame(PaintPreviewFrame&& other) = default;
PaintPreviewFrame& PaintPreviewFrame::operator=(PaintPreviewFrame&& other) =
    default;

}  // namespace paint_preview
