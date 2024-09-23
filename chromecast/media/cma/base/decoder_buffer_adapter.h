// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BASE_DECODER_BUFFER_ADAPTER_H_
#define CHROMECAST_MEDIA_CMA_BASE_DECODER_BUFFER_ADAPTER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "chromecast/media/api/decoder_buffer_base.h"

namespace media {
class DecoderBuffer;
}

namespace chromecast {
namespace media {

// DecoderBufferAdapter wraps a ::media::DecoderBuffer
// into a DecoderBufferBase.
class DecoderBufferAdapter : public DecoderBufferBase {
 public:
  // Using explicit constructor without providing stream Id will set it to
  // kPrimary by default.
  explicit DecoderBufferAdapter(
      const scoped_refptr<::media::DecoderBuffer>& buffer);
  DecoderBufferAdapter(StreamId stream_id,
                       const scoped_refptr<::media::DecoderBuffer>& buffer);

  DecoderBufferAdapter(const DecoderBufferAdapter&) = delete;
  DecoderBufferAdapter& operator=(const DecoderBufferAdapter&) = delete;

  // DecoderBufferBase implementation:
  StreamId stream_id() const override;
  int64_t timestamp() const override;
  void set_timestamp(base::TimeDelta timestamp) override;
  const uint8_t* data() const override;
  uint8_t* writable_data() const override;
  size_t data_size() const override;
  const CastDecryptConfig* decrypt_config() const override;
  bool end_of_stream() const override;
  bool is_key_frame() const override;

 private:
  ~DecoderBufferAdapter() override;

  StreamId stream_id_;
  scoped_refptr<::media::DecoderBuffer> const buffer_;
  std::unique_ptr<CastDecryptConfig> decrypt_config_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BASE_DECODER_BUFFER_ADAPTER_H_
