// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_API_DECODER_BUFFER_BASE_H_
#define CHROMECAST_MEDIA_API_DECODER_BUFFER_BASE_H_

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "chromecast/public/media/cast_decoder_buffer.h"
#include "chromecast/public/media/decrypt_context.h"

namespace chromecast {
namespace media {

// DecoderBufferBase exposes only the properties of an audio/video buffer.
// The way a DecoderBufferBase is created and organized in memory
// is left as a detail of the implementation of derived classes.
class DecoderBufferBase : public CastDecoderBuffer,
                          public base::RefCountedThreadSafe<DecoderBufferBase> {
 public:
  // Partial CastDecoderBuffer implementation:
  DecryptContext* decrypt_context() const override;

  void set_decrypt_context(std::unique_ptr<DecryptContext> context) {
    decrypt_context_ = std::move(context);
  }

  // Sets the PTS of the frame.
  virtual void set_timestamp(base::TimeDelta timestamp) = 0;

  // Gets a pointer to the frame data buffer.
  virtual uint8_t* writable_data() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<DecoderBufferBase>;

  DecoderBufferBase();
  ~DecoderBufferBase() override;

 private:
  std::unique_ptr<DecryptContext> decrypt_context_;

  DecoderBufferBase(const DecoderBufferBase&) = delete;
  DecoderBufferBase& operator=(const DecoderBufferBase&) = delete;
};

inline DecoderBufferBase::DecoderBufferBase() {}

inline DecoderBufferBase::~DecoderBufferBase() {}

inline DecryptContext* DecoderBufferBase::decrypt_context() const {
  return decrypt_context_.get();
}

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_API_DECODER_BUFFER_BASE_H_
