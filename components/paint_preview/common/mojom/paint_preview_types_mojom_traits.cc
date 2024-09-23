// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/mojom/paint_preview_types_mojom_traits.h"
#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"

namespace mojo {

// static
base::File UnionTraits<paint_preview::mojom::SerializedRecordingDataView,
                       paint_preview::SerializedRecording>::
    file(paint_preview::SerializedRecording& serialized_recording) {
  return std::move(serialized_recording.file_);
}

// static
mojo_base::BigBuffer
UnionTraits<paint_preview::mojom::SerializedRecordingDataView,
            paint_preview::SerializedRecording>::
    buffer(paint_preview::SerializedRecording& serialized_recording) {
  return std::move(serialized_recording.buffer_.value());
}

// static
bool UnionTraits<paint_preview::mojom::SerializedRecordingDataView,
                 paint_preview::SerializedRecording>::
    Read(paint_preview::mojom::SerializedRecordingDataView data,
         paint_preview::SerializedRecording* out) {
  if (data.is_file()) {
    base::File file;
    if (!data.ReadFile(&file))
      return false;
    *out = paint_preview::SerializedRecording(std::move(file));
    return true;
  } else if (data.is_buffer()) {
    mojo_base::BigBuffer buffer;
    if (!data.ReadBuffer(&buffer))
      return false;
    *out = paint_preview::SerializedRecording(std::move(buffer));
    return true;
  } else {
    return false;
  }
}

// static
paint_preview::mojom::SerializedRecordingDataView::Tag
UnionTraits<paint_preview::mojom::SerializedRecordingDataView,
            paint_preview::SerializedRecording>::
    GetTag(const paint_preview::SerializedRecording& serialized_recording) {
  switch (serialized_recording.persistence_) {
    case paint_preview::RecordingPersistence::kFileSystem:
      return paint_preview::mojom::SerializedRecordingDataView::Tag::kFile;
    case paint_preview::RecordingPersistence::kMemoryBuffer:
      return paint_preview::mojom::SerializedRecordingDataView::Tag::kBuffer;
  }

  NOTREACHED_IN_MIGRATION();
  return paint_preview::mojom::SerializedRecordingDataView::Tag::kFile;
}

}  // namespace mojo
