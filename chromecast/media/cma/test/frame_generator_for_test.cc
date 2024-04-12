// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/test/frame_generator_for_test.h"

#include <stdint.h>

#include <utility>

#include "chromecast/media/api/decoder_buffer_base.h"
#include "chromecast/media/cma/base/decoder_buffer_adapter.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"

namespace chromecast {
namespace media {

FrameGeneratorForTest::FrameSpec::FrameSpec()
    : has_config(false),
      is_eos(false),
      has_decrypt_config(false),
      size(0) {
}

FrameGeneratorForTest::FrameSpec::~FrameSpec() {
}

FrameGeneratorForTest::FrameGeneratorForTest(
    const std::vector<FrameSpec> frame_specs)
  : frame_specs_(frame_specs),
    frame_idx_(0),
    total_buffer_size_(0) {
}

FrameGeneratorForTest::~FrameGeneratorForTest() {
}

bool FrameGeneratorForTest::HasDecoderConfig() const {
  if (frame_idx_ >= frame_specs_.size())
    return false;

  return frame_specs_[frame_idx_].has_config;
}

scoped_refptr<DecoderBufferBase> FrameGeneratorForTest::Generate() {
  if (frame_idx_ >= frame_specs_.size())
    return scoped_refptr<DecoderBufferBase>();

  const FrameSpec& frame_spec = frame_specs_[frame_idx_];
  frame_idx_++;

  if (frame_spec.is_eos) {
    return scoped_refptr<DecoderBufferBase>(
        new DecoderBufferAdapter(::media::DecoderBuffer::CreateEOSBuffer()));
  }

  scoped_refptr< ::media::DecoderBuffer> buffer(
      new ::media::DecoderBuffer(frame_spec.size));

  // Timestamp.
  buffer->set_timestamp(frame_spec.timestamp);

  // Generate the frame data.
  for (size_t k = 0; k < frame_spec.size; k++) {
    buffer->writable_data()[k] = total_buffer_size_ & 0xff;
    total_buffer_size_++;
  }

  // Generate the decrypt configuration.
  if (frame_spec.has_decrypt_config) {
    uint32_t frame_size = buffer->size();
    uint32_t chunk_size = 1;
    std::vector< ::media::SubsampleEntry> subsamples;
    while (frame_size > 0) {
      ::media::SubsampleEntry subsample;
      subsample.clear_bytes = chunk_size;
      if (subsample.clear_bytes > frame_size)
        subsample.clear_bytes = frame_size;
      frame_size -= subsample.clear_bytes;
      chunk_size <<= 1;

      subsample.cypher_bytes = chunk_size;
      if (subsample.cypher_bytes > frame_size)
        subsample.cypher_bytes = frame_size;
      frame_size -= subsample.cypher_bytes;
      chunk_size <<= 1;

      subsamples.push_back(subsample);
    }

    char key_id[] = {
      0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
      0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf };

    char iv[] = {
      0x0, 0x2, 0x1, 0x3, 0x5, 0x4, 0x7, 0x6,
      0x9, 0x8, 0xb, 0xa, 0xd, 0xc, 0xf, 0xe };

    std::unique_ptr<::media::DecryptConfig> decrypt_config =
        ::media::DecryptConfig::CreateCencConfig(
            std::string(key_id, std::size(key_id)),
            std::string(iv, std::size(iv)), subsamples);
    buffer->set_decrypt_config(std::move(decrypt_config));
  }

  return scoped_refptr<DecoderBufferBase>(new DecoderBufferAdapter(buffer));
}

size_t FrameGeneratorForTest::RemainingFrameCount() const {
  size_t count = frame_specs_.size() - frame_idx_;
  return count;
}

}  // namespace media
}  // namespace chromecast
