// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_PLAYER_COMPOSITOR_STATUS_H_
#define COMPONENTS_PAINT_PREVIEW_PLAYER_COMPOSITOR_STATUS_H_

namespace paint_preview {

// IMPORTANT: if CompositorStatus is updated, please update the corresponding
// entry for TabbedPaintPreviewCompositorFailureReason in enums.xml.

// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.paintpreview.player)
enum class CompositorStatus : int {
  OK,
  URL_MISMATCH,
  COMPOSITOR_SERVICE_DISCONNECT,
  COMPOSITOR_CLIENT_DISCONNECT,
  PROTOBUF_DESERIALIZATION_ERROR,
  COMPOSITOR_DESERIALIZATION_ERROR,
  INVALID_ROOT_FRAME_SKP,
  INVALID_REQUEST,
  OLD_VERSION,
  UNEXPECTED_VERSION,
  CAPTURE_EXPIRED,
  NO_CAPTURE,
  TIMED_OUT,
  STOPPED_DUE_TO_MEMORY_PRESSURE,
  SKIPPED_DUE_TO_MEMORY_PRESSURE,
  // Used by long screenshots code only when call to requestBitmap fails.
  REQUEST_BITMAP_FAILURE,
  COUNT,
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_PLAYER_COMPOSITOR_STATUS_H_
