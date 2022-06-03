// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_BASE_CAST_DECODER_BUFFER_IMPL_H_
#define CHROMECAST_MEDIA_BASE_CAST_DECODER_BUFFER_IMPL_H_

#include "base/time/time.h"
#include "chromecast/media/api/decoder_buffer_base.h"

namespace chromecast {
namespace media {

class CastDecoderBufferImpl : public DecoderBufferBase {
 public:
  // Using explicit constructor without providing stream id will set it to
  // kPrimary by default.
  explicit CastDecoderBufferImpl(size_t size);
  CastDecoderBufferImpl(size_t size, StreamId stream_id);

  static scoped_refptr<CastDecoderBufferImpl> CreateEOSBuffer();
  static scoped_refptr<CastDecoderBufferImpl> CreateEOSBuffer(
      StreamId stream_id);

  // DecoderBufferBase implementation:
  StreamId stream_id() const override;
  int64_t timestamp() const override;
  void set_timestamp(base::TimeDelta timestamp) override;
  const uint8_t* data() const override;
  uint8_t* writable_data() const override;
  size_t data_size() const override;
  const CastDecryptConfig* decrypt_config() const override;
  bool end_of_stream() const override;

 private:
  // This constructor is used by CreateEOSBuffer.
  explicit CastDecoderBufferImpl(StreamId stream_id);
  ~CastDecoderBufferImpl() override;

  const StreamId stream_id_;
  const size_t size_;
  const std::unique_ptr<uint8_t[]> data_;
  base::TimeDelta timestamp_;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_BASE_CAST_DECODER_BUFFER_IMPL_H_
