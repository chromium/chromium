// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/image_fetcher/core/image_decoder.h"

#include <utility>

#include "base/memory/ref_counted_memory.h"

namespace image_fetcher {

void ImageDecoder::DecodeImageFromString(
    const std::string& image_data,
    const gfx::Size& desired_image_frame_size,
    data_decoder::DataDecoder* data_decoder,
    ImageDecodedCallback callback) {
  DecodeImage(base::MakeRefCounted<base::RefCountedString>(image_data),
              desired_image_frame_size, data_decoder, std::move(callback));
}

}  // namespace image_fetcher
