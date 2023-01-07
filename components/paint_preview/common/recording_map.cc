// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/recording_map.h"

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace paint_preview {

namespace {

base::FilePath ToFilePath(base::StringPiece path_str) {
#if BUILDFLAG(IS_WIN)
  return base::FilePath(base::UTF8ToWide(path_str));
#else
  return base::FilePath(path_str);
#endif
}

RecordingMap RecordingMapFromBufferMap(
    base::flat_map<base::UnguessableToken, mojo_base::BigBuffer>&& buffer_map) {
  std::vector<std::pair<base::UnguessableToken, SerializedRecording>> entries;
  for (auto& entry : buffer_map) {
    entries.emplace_back(entry.first,
                         SerializedRecording(std::move(entry.second)));
  }
  return RecordingMap(std::move(entries));
}

}  // namespace

std::pair<RecordingMap, PaintPreviewProto> RecordingMapFromCaptureResult(
    CaptureResult&& capture_result) {
  switch (capture_result.persistence) {
    case RecordingPersistence::kFileSystem:
      return {RecordingMapFromPaintPreviewProto(capture_result.proto),
              std::move(capture_result.proto)};
    case RecordingPersistence::kMemoryBuffer:
      return {
          RecordingMapFromBufferMap(std::move(capture_result.serialized_skps)),
          capture_result.proto};
  }

  NOTREACHED();
  return {};
}

RecordingMap RecordingMapFromPaintPreviewProto(const PaintPreviewProto& proto) {
  std::vector<std::pair<base::UnguessableToken, SerializedRecording>> entries;
  entries.reserve(1 + proto.subframes_size());

  SerializedRecording root_frame_recording(
      ToFilePath(proto.root_frame().file_path()));

  // We can't composite anything with an invalid SKP file path for the root
  // frame.
  if (!root_frame_recording.IsValid())
    return {};

  entries.emplace_back(base::UnguessableToken::Deserialize(
                           proto.root_frame().embedding_token_high(),
                           proto.root_frame().embedding_token_low()),
                       std::move(root_frame_recording));

  for (const auto& subframe : proto.subframes()) {
    SerializedRecording frame_recording(ToFilePath(subframe.file_path()));

    // Skip this frame if it doesn't have a valid SKP file path.
    if (!frame_recording.IsValid())
      continue;

    entries.emplace_back(
        base::UnguessableToken::Deserialize(subframe.embedding_token_high(),
                                            subframe.embedding_token_low()),
        std::move(frame_recording));
  }

  return RecordingMap(std::move(entries));
}

}  // namespace paint_preview
