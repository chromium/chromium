// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/capture_result.h"

namespace paint_preview {

RecordingParams::RecordingParams(const base::UnguessableToken& document_guid)
    : is_main_frame(false),
      capture_links(true),
      max_capture_size(0),
      document_guid(document_guid) {}

RecordingParams::RecordingParams(RecordingParams&&) = default;
RecordingParams& RecordingParams::operator=(RecordingParams&&) = default;

RecordingParams RecordingParams::Clone() const {
  RecordingParams copy(document_guid);

  copy.clip_rect = clip_rect;
  copy.clip_x_coord_override = clip_x_coord_override;
  copy.clip_y_coord_override = clip_y_coord_override;
  copy.is_main_frame = is_main_frame;
  copy.capture_links = capture_links;
  copy.max_capture_size = max_capture_size;
  copy.max_decoded_image_size_bytes = max_decoded_image_size_bytes;
  copy.skip_accelerated_content = skip_accelerated_content;
  copy.redaction_params = redaction_params;

  return copy;
}

CaptureResult::CaptureResult(RecordingPersistence persistence)
    : persistence(persistence) {}

CaptureResult::~CaptureResult() = default;

CaptureResult::CaptureResult(CaptureResult&&) = default;

CaptureResult& CaptureResult::operator=(CaptureResult&&) = default;

}  // namespace paint_preview
