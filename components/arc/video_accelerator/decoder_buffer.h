// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_VIDEO_ACCELERATOR_DECODER_BUFFER_H_
#define COMPONENTS_ARC_VIDEO_ACCELERATOR_DECODER_BUFFER_H_

#include "base/files/scoped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"

namespace media {
class DecoderBuffer;
}  // namespace media

namespace arc {

// Intermediate class in converting from a mojom::DecoderBuffer to
// media::DecoderBuffer. Because media::DecoderBuffer doesn't have a public
// constructor, we cannot convert to media::DecoderBuffer directly by
// StructTraits::Read().
struct DecoderBuffer {
  DecoderBuffer();
  DecoderBuffer(base::ScopedFD handle_fd,
                uint32_t offset,
                uint32_t payload_size,
                bool end_of_stream,
                base::TimeDelta timestamp);
  ~DecoderBuffer();
  DecoderBuffer(DecoderBuffer&& buf);
  DecoderBuffer& operator=(DecoderBuffer&& buf);

  // Convert to media::DecoderBuffer.
  scoped_refptr<media::DecoderBuffer> ToMediaDecoderBuffer() &&;

  // See components/arc/mojom/video_common.mojom for descriptions of each field.
  base::ScopedFD handle_fd;
  uint32_t offset;
  uint32_t payload_size;
  bool end_of_stream;
  base::TimeDelta timestamp;
};

}  // namespace arc

#endif  // COMPONENTS_ARC_VIDEO_ACCELERATOR_DECODER_BUFFER_H_
