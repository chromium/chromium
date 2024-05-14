// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_MOJOM_PAINT_PREVIEW_TYPES_MOJOM_TRAITS_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_MOJOM_PAINT_PREVIEW_TYPES_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "components/paint_preview/common/mojom/paint_preview_types.mojom-shared.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"

namespace mojo {

template <>
struct EnumTraits<paint_preview::mojom::RecordingPersistence,
                  paint_preview::RecordingPersistence> {
  static paint_preview::mojom::RecordingPersistence ToMojom(
      paint_preview::RecordingPersistence persistence) {
    switch (persistence) {
      case paint_preview::RecordingPersistence::kFileSystem:
        return paint_preview::mojom::RecordingPersistence::kFileSystem;
      case paint_preview::RecordingPersistence::kMemoryBuffer:
        return paint_preview::mojom::RecordingPersistence::kMemoryBuffer;
    }
    NOTREACHED_IN_MIGRATION()
        << "Unknown persistence " << static_cast<int>(persistence);
    return paint_preview::mojom::RecordingPersistence::kFileSystem;
  }

  static bool FromMojom(paint_preview::mojom::RecordingPersistence input,
                        paint_preview::RecordingPersistence* output) {
    switch (input) {
      case paint_preview::mojom::RecordingPersistence::kFileSystem:
        *output = paint_preview::RecordingPersistence::kFileSystem;
        return true;
      case paint_preview::mojom::RecordingPersistence::kMemoryBuffer:
        *output = paint_preview::RecordingPersistence::kMemoryBuffer;
        return true;
    }
    return false;
  }
};

template <>
class UnionTraits<paint_preview::mojom::SerializedRecordingDataView,
                  paint_preview::SerializedRecording> {
 public:
  static base::File file(
      paint_preview::SerializedRecording& serialized_recording);

  static mojo_base::BigBuffer buffer(
      paint_preview::SerializedRecording& serialized_recording);

  static bool Read(paint_preview::mojom::SerializedRecordingDataView data,
                   paint_preview::SerializedRecording* out);

  static paint_preview::mojom::SerializedRecordingDataView::Tag GetTag(
      const paint_preview::SerializedRecording& serialized_recording);
};

}  // namespace mojo

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_MOJOM_PAINT_PREVIEW_TYPES_MOJOM_TRAITS_H_
