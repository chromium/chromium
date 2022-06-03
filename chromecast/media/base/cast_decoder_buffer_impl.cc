// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/base/cast_decoder_buffer_impl.h"

namespace chromecast {
namespace media {

CastDecoderBufferImpl::CastDecoderBufferImpl(size_t size, StreamId stream_id)
    : stream_id_(stream_id),
      size_(size),
      data_(std::make_unique<uint8_t[]>(size_)) {}

CastDecoderBufferImpl::CastDecoderBufferImpl(size_t size)
    : CastDecoderBufferImpl(size, kPrimary) {}

CastDecoderBufferImpl::CastDecoderBufferImpl(StreamId stream_id)
    : stream_id_(stream_id), size_(0), data_(nullptr) {}

CastDecoderBufferImpl::~CastDecoderBufferImpl() = default;

// static
scoped_refptr<CastDecoderBufferImpl> CastDecoderBufferImpl::CreateEOSBuffer() {
  return CreateEOSBuffer(kPrimary);
}

// static
scoped_refptr<CastDecoderBufferImpl> CastDecoderBufferImpl::CreateEOSBuffer(
    StreamId stream_id) {
  return base::WrapRefCounted(new CastDecoderBufferImpl(stream_id));
}

StreamId CastDecoderBufferImpl::stream_id() const {
  return stream_id_;
}

int64_t CastDecoderBufferImpl::timestamp() const {
  DCHECK(!end_of_stream());
  return timestamp_.InMicroseconds();
}

void CastDecoderBufferImpl::set_timestamp(base::TimeDelta timestamp) {
  DCHECK(!end_of_stream());
  timestamp_ = timestamp;
}

const uint8_t* CastDecoderBufferImpl::data() const {
  DCHECK(!end_of_stream());
  return data_.get();
}

uint8_t* CastDecoderBufferImpl::writable_data() const {
  DCHECK(!end_of_stream());
  return data_.get();
}

size_t CastDecoderBufferImpl::data_size() const {
  DCHECK(!end_of_stream());
  return size_;
}

const CastDecryptConfig* CastDecoderBufferImpl::decrypt_config() const {
  return nullptr;
}

bool CastDecoderBufferImpl::end_of_stream() const {
  return data_ == nullptr;
}

}  // namespace media
}  // namespace chromecast
