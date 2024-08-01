// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/paint_preview/common/serialized_recording.h"

#include <optional>

#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "components/paint_preview/common/serial_utils.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/skia/include/core/SkStream.h"

namespace paint_preview {

namespace {

// Serializes |skp| to |out_stream| as an SkPicture of size |dimensions|.
// |tracker| supplies metadata required during serialization.
bool SerializeSkPicture(sk_sp<const SkPicture> skp,
                        PaintPreviewTracker* tracker,
                        SkWStream* out_stream) {
  TypefaceSerializationContext typeface_context(tracker->GetTypefaceUsageMap());
  ImageSerializationContext* image_context =
      tracker->GetImageSerializationContext();
  auto serial_procs = MakeSerialProcs(tracker->GetPictureSerializationContext(),
                                      &typeface_context, image_context);

  skp->serialize(out_stream, &serial_procs);
  out_stream->flush();

  // If the memory budget was exceeded while serializing images and it is not
  // tolerated (inferred from setting a max decoded image size) then abort.
  const bool tolerates_discarding =
      image_context->max_decoded_image_size_bytes !=
      std::numeric_limits<uint64_t>::max();
  return tolerates_discarding || !image_context->memory_budget_exceeded;
}

}  // namespace

SkpResult::SkpResult() = default;
SkpResult::~SkpResult() = default;

SkpResult::SkpResult(SkpResult&& other) = default;
SkpResult& SkpResult::operator=(SkpResult&& rhs) = default;

SerializedRecording::SerializedRecording()
    : persistence_(RecordingPersistence::kFileSystem), file_(), buffer_() {}

SerializedRecording::SerializedRecording(base::FilePath path)
    : SerializedRecording(
          base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ)) {}

SerializedRecording::SerializedRecording(base::File file)
    : persistence_(RecordingPersistence::kFileSystem),
      file_(std::move(file)),
      buffer_() {}

SerializedRecording::SerializedRecording(mojo_base::BigBuffer buffer)
    : persistence_(RecordingPersistence::kMemoryBuffer),
      file_(),
      buffer_(std::move(buffer)) {}

SerializedRecording::SerializedRecording(SerializedRecording&&) = default;

SerializedRecording& SerializedRecording::operator=(SerializedRecording&&) =
    default;

SerializedRecording::~SerializedRecording() {
  if (is_file() && file_.IsValid()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
        base::BindOnce([](base::File file) { file.Close(); },
                       std::move(file_)));
  }
}

bool SerializedRecording::IsValid() const {
  if (is_file()) {
    return file_.IsValid();
  } else if (is_buffer()) {
    return buffer_.has_value();
  } else {
    NOTREACHED_IN_MIGRATION();
    return false;
  }
}

std::optional<SkpResult> SerializedRecording::Deserialize() && {
  TRACE_EVENT0("paint_preview", "SerializedRecording::Deserialize");
  SkpResult result;
  SkDeserialProcs procs = MakeDeserialProcs(&result.ctx);

  if (is_file()) {
    FileRStream stream(std::move(file_));
    result.skp = SkPicture::MakeFromStream(&stream, &procs);
  } else if (is_buffer()) {
    CHECK(buffer_.has_value());
    SkMemoryStream stream(buffer_->data(), buffer_->size(),
                          /*copyData=*/false);
    result.skp = SkPicture::MakeFromStream(&stream, &procs);
  } else {
    NOTREACHED_IN_MIGRATION();
    return {};
  }

  return {std::move(result)};
}

sk_sp<SkPicture> SerializedRecording::DeserializeWithContext(
    LoadedFramesDeserialContext* ctx) && {
  TRACE_EVENT0("paint_preview", "SerializedRecording::DeserializeWithContext");

  SkDeserialProcs procs = MakeDeserialProcs(ctx);

  if (is_file()) {
    FileRStream stream(std::move(file_));
    return SkPicture::MakeFromStream(&stream, &procs);
  } else if (is_buffer()) {
    CHECK(buffer_.has_value());
    SkMemoryStream stream(buffer_->data(), buffer_->size(),
                          /*copyData=*/false);
    return SkPicture::MakeFromStream(&stream, &procs);
  } else {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
}

bool RecordToFile(base::File file,
                  sk_sp<const SkPicture> skp,
                  PaintPreviewTracker* tracker,
                  std::optional<size_t> max_capture_size,
                  size_t* serialized_size) {
  if (!file.IsValid())
    return false;

  if (max_capture_size.has_value() && max_capture_size.value() == 0)
    return false;

  FileWStream file_stream(std::move(file), max_capture_size.value_or(0));
  if (!SerializeSkPicture(skp, tracker, &file_stream))
    return false;

  file_stream.Close();
  *serialized_size = file_stream.ActualBytesWritten();
  return !file_stream.DidWriteFail();
}

std::optional<mojo_base::BigBuffer> RecordToBuffer(
    sk_sp<const SkPicture> skp,
    PaintPreviewTracker* tracker,
    std::optional<size_t> maybe_max_capture_size,
    size_t* serialized_size) {
  SkDynamicMemoryWStream memory_stream;
  if (!SerializeSkPicture(skp, tracker, &memory_stream))
    return std::nullopt;

  size_t max_capture_size = maybe_max_capture_size.value_or(SIZE_MAX);
  if (max_capture_size == 0)
    return std::nullopt;

  sk_sp<SkData> data = memory_stream.detachAsData();
  *serialized_size = std::min(data->size(), max_capture_size);
  mojo_base::BigBuffer buffer(
      base::span<const uint8_t>(data->bytes(), *serialized_size));
  if (data->size() > max_capture_size)
    return std::nullopt;

  return {std::move(buffer)};
}

}  // namespace paint_preview
