// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/capture_result.h"

namespace paint_preview {

RecordingParams::RecordingParams(const base::UnguessableToken& document_guid)
    : document_guid(document_guid),
      is_main_frame(false),
      capture_links(true),
      max_capture_size(0) {}

CaptureResult::CaptureResult(RecordingPersistence persistence)
    : persistence(persistence) {}

CaptureResult::~CaptureResult() = default;

CaptureResult::CaptureResult(CaptureResult&&) = default;

CaptureResult& CaptureResult::operator=(CaptureResult&&) = default;

}  // namespace paint_preview
