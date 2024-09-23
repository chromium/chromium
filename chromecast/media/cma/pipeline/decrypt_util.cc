// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/decrypt_util.h"

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "chromecast/media/base/decrypt_context_impl.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "media/base/decoder_buffer.h"

namespace chromecast {
namespace media {
namespace {
void OnBufferDecrypted(scoped_refptr<DecoderBufferBase> buffer,
                       BufferDecryptedCB buffer_decrypted_cb,
                       bool success) {
  scoped_refptr<DecoderBufferBase> out_buffer =
      success ? base::MakeRefCounted<DecoderBufferClear>(buffer) : buffer;
  std::move(buffer_decrypted_cb).Run(std::move(out_buffer), success);
}
}  // namespace

DecoderBufferClear::DecoderBufferClear(scoped_refptr<DecoderBufferBase> buffer)
    : buffer_(buffer) {}

DecoderBufferClear::~DecoderBufferClear() {}

StreamId DecoderBufferClear::stream_id() const {
  return buffer_->stream_id();
}

int64_t DecoderBufferClear::timestamp() const {
  return buffer_->timestamp();
}

void DecoderBufferClear::set_timestamp(base::TimeDelta timestamp) {
  buffer_->set_timestamp(timestamp);
}

const uint8_t* DecoderBufferClear::data() const {
  return buffer_->data();
}

uint8_t* DecoderBufferClear::writable_data() const {
  return buffer_->writable_data();
}

size_t DecoderBufferClear::data_size() const {
  return buffer_->data_size();
}

const CastDecryptConfig* DecoderBufferClear::decrypt_config() const {
  // Buffer is clear so no decryption info.
  return nullptr;
}

bool DecoderBufferClear::end_of_stream() const {
  return buffer_->end_of_stream();
}

bool DecoderBufferClear::is_key_frame() const {
  return buffer_->is_key_frame();
}

void DecryptDecoderBuffer(scoped_refptr<DecoderBufferBase> buffer,
                          DecryptContextImpl* decrypt_ctxt,
                          BufferDecryptedCB buffer_decrypted_cb) {
  decrypt_ctxt->DecryptAsync(buffer.get(), buffer->writable_data(),
                             0 /* data_offset */, true /* clear_output */,
                             base::BindOnce(&OnBufferDecrypted, buffer,
                                            std::move(buffer_decrypted_cb)));
}

}  // namespace media
}  // namespace chromecast
