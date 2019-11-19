// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/video_accelerator/decoder_buffer.h"

#include "base/logging.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/numerics/checked_math.h"
#include "base/unguessable_token.h"
#include "components/arc/video_accelerator/arc_video_accelerator_util.h"
#include "media/base/decoder_buffer.h"
#include "media/gpu/buffer_validation.h"

namespace arc {

DecoderBuffer::DecoderBuffer() = default;

DecoderBuffer::DecoderBuffer(base::ScopedFD handle_fd,
                             uint32_t offset,
                             uint32_t payload_size,
                             bool end_of_stream,
                             base::TimeDelta timestamp)
    : handle_fd(std::move(handle_fd)),
      offset(offset),
      payload_size(payload_size),
      end_of_stream(end_of_stream),
      timestamp(timestamp) {}

DecoderBuffer::DecoderBuffer(DecoderBuffer&& buf) = default;

DecoderBuffer& DecoderBuffer::operator=(DecoderBuffer&& buf) = default;

DecoderBuffer::~DecoderBuffer() = default;

scoped_refptr<media::DecoderBuffer> DecoderBuffer::ToMediaDecoderBuffer() && {
  if (end_of_stream)
    return media::DecoderBuffer::CreateEOSBuffer();

  base::CheckedNumeric<off_t> checked_offset(offset);
  base::CheckedNumeric<size_t> checked_payload_size(payload_size);
  if (!checked_offset.IsValid() || !checked_payload_size.IsValid()) {
    VLOG(1) << "Overflow when convert offset and payload size to size_t.";
    return nullptr;
  }

  size_t required_size;
  if (!base::CheckAdd<size_t>(offset, payload_size)
           .AssignIfValid(&required_size)) {
    VLOG(1) << "Overflow when adding offset and payload size.";
    return nullptr;
  }

  size_t file_size = 0;
  if (!media::GetFileSize(handle_fd.get(), &file_size) ||
      file_size < required_size) {
    VLOG(1) << "File size(" << file_size << ") is smaller than required size("
            << required_size << ").";
    return nullptr;
  }

  DCHECK(handle_fd.is_valid());
  auto readonly_region = base::ReadOnlySharedMemoryRegion::Deserialize(
      base::subtle::PlatformSharedMemoryRegion::Take(
          std::move(handle_fd),
          base::subtle::PlatformSharedMemoryRegion::Mode::kReadOnly, file_size,
          base::UnguessableToken::Create()));

  scoped_refptr<media::DecoderBuffer> output =
      media::DecoderBuffer::FromSharedMemoryRegion(
          std::move(readonly_region), checked_offset.ValueOrDie(),
          checked_payload_size.ValueOrDie());
  output->set_timestamp(timestamp);
  return output;
}

}  // namespace arc
