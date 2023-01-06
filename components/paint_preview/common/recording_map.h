// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_RECORDING_MAP_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_RECORDING_MAP_H_

#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "components/paint_preview/common/capture_result.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace paint_preview {

// A map of rendering frame embedding tokens to serialized |SkPicture|s.
using RecordingMap =
    base::flat_map<base::UnguessableToken, SerializedRecording>;

// Create and populate recording map using |result|'s persistence value.
// Note: this calls |RecordingMapFromPaintPreviewProto| in the case of
// |RecordingPersistence::kFileSystem|.
std::pair<RecordingMap, PaintPreviewProto> RecordingMapFromCaptureResult(
    CaptureResult&& result);

// Create files and populate a recording map based on |proto|. Note that |proto|
// must have been created with |RecordingPersistence::kFileSystem|.
RecordingMap RecordingMapFromPaintPreviewProto(const PaintPreviewProto& proto);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_RECORDING_MAP_H_
