// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_CAST_DECODER_BUFFER_H_
#define CHROMECAST_PUBLIC_MEDIA_CAST_DECODER_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include "stream_id.h"

namespace chromecast {
namespace media {

class CastDecryptConfig;
class DecryptContext;

// CastDecoderBuffer provides an interface for passing a single frame of audio
// or video data to the pipeline backend.  End-of-stream is indicated by passing
// a frame where end_of_stream() returns true.
// The buffer's lifetime is managed by the caller code - it MUST NOT be
// deleted by the MediaPipelineBackend implementation, and MUST NOT be
// dereferenced after completion of buffer push (i.e.
// kBufferSuccess/kBufferFailure for synchronous completion, OnPushComplete
// for kBufferPending case).
// TODO(halliwell): consider renaming functions here to camel case.
class CastDecoderBuffer {
 public:
  virtual ~CastDecoderBuffer() {}

  // Returns the stream id of this decoder buffer.
  virtual StreamId stream_id() const = 0;

  // Returns the PTS of the frame in microseconds.
  virtual int64_t timestamp() const = 0;

  // Gets the frame data.
  virtual const uint8_t* data() const = 0;

  // Returns the size of the frame in bytes.
  virtual size_t data_size() const = 0;

  // Returns the decrypt configuration.
  // Returns nullptr if and only if the buffer is unencrypted.
  virtual const CastDecryptConfig* decrypt_config() const = 0;

  // Returns the decrypt context. Returns nullptr if and only if the buffer is
  // unencrypted.
  virtual DecryptContext* decrypt_context() const = 0;

  // Indicates if this is a special frame that indicates the end of the stream.
  // If true, functions to access the frame content cannot be called.
  virtual bool end_of_stream() const = 0;

  // Indicates if this is a key frame. Only relevant to buffers containing video
  // data.
  virtual bool is_key_frame() const = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_CAST_DECODER_BUFFER_H_
