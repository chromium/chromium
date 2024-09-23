// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/base/decoder_buffer_adapter.h"

#include "base/notreached.h"
#include "chromecast/media/cma/base/cast_decrypt_config_impl.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "media/base/decoder_buffer.h"

namespace chromecast {
namespace media {

namespace {

// Converts a chromium EncryptionScheme to a cast one.
EncryptionScheme ToEncryptionScheme(::media::EncryptionScheme scheme) {
  switch (scheme) {
    case ::media::EncryptionScheme::kUnencrypted:
      return EncryptionScheme::kUnencrypted;
    case ::media::EncryptionScheme::kCenc:
      return EncryptionScheme::kAesCtr;
    case ::media::EncryptionScheme::kCbcs:
      return EncryptionScheme::kAesCbc;
    default:
      NOTREACHED();
  }
}

}  // namespace

DecoderBufferAdapter::DecoderBufferAdapter(
    const scoped_refptr<::media::DecoderBuffer>& buffer)
    : DecoderBufferAdapter(kPrimary, buffer) {}

DecoderBufferAdapter::DecoderBufferAdapter(
    StreamId stream_id,
    const scoped_refptr<::media::DecoderBuffer>& buffer)
    : stream_id_(stream_id), buffer_(buffer) {
  DCHECK(buffer_);

  const ::media::DecryptConfig* decrypt_config =
      buffer_->end_of_stream() ? nullptr : buffer_->decrypt_config();
  if (decrypt_config) {
    std::vector<SubsampleEntry> subsamples;
    for (const auto& sample : decrypt_config->subsamples()) {
      subsamples.emplace_back(sample.clear_bytes, sample.cypher_bytes);
    }
    if (subsamples.empty()) {
      // DecryptConfig may contain 0 subsamples if all content is encrypted.
      // Map this case to a single fully-encrypted "subsample" for more
      // consistent backend handling.
      subsamples.emplace_back(0, buffer_->size());
    }

    EncryptionPattern pattern;
    if (decrypt_config->encryption_pattern()) {
      pattern = EncryptionPattern(
          decrypt_config->encryption_pattern()->crypt_byte_block(),
          decrypt_config->encryption_pattern()->skip_byte_block());
    }

    decrypt_config_.reset(new CastDecryptConfigImpl(
        decrypt_config->key_id(), decrypt_config->iv(), pattern,
        std::move(subsamples),
        ToEncryptionScheme(decrypt_config->encryption_scheme())));
  }
}

DecoderBufferAdapter::~DecoderBufferAdapter() {}

StreamId DecoderBufferAdapter::stream_id() const {
  return stream_id_;
}

int64_t DecoderBufferAdapter::timestamp() const {
  return buffer_->timestamp().InMicroseconds();
}

void DecoderBufferAdapter::set_timestamp(base::TimeDelta timestamp) {
  buffer_->set_timestamp(timestamp);
}

const uint8_t* DecoderBufferAdapter::data() const {
  return buffer_->data();
}

uint8_t* DecoderBufferAdapter::writable_data() const {
  return buffer_->writable_data();
}

size_t DecoderBufferAdapter::data_size() const {
  return buffer_->size();
}

const CastDecryptConfig* DecoderBufferAdapter::decrypt_config() const {
  return decrypt_config_.get();
}

bool DecoderBufferAdapter::end_of_stream() const {
  return buffer_->end_of_stream();
}

bool DecoderBufferAdapter::is_key_frame() const {
  return buffer_->is_key_frame();
}

}  // namespace media
}  // namespace chromecast
