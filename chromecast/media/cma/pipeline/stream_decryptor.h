// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_PIPELINE_STREAM_DECRYPTOR_H_
#define CHROMECAST_MEDIA_CMA_PIPELINE_STREAM_DECRYPTOR_H_

#include <queue>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"

namespace chromecast {
namespace media {
class DecoderBufferBase;

// Decryptor to get clear buffers in asynchronous way. All the buffers are
// pushed into decryptor to keep the order of frames.
class StreamDecryptor {
 public:
  using BufferQueue = std::queue<scoped_refptr<DecoderBufferBase>>;

  // Callback for Decrypt. The first argument is true iff Decrypt was
  // successful. The second argument contains all the buffers that are ready to
  // be pushed to decoder by the time the callback is called.
  // If BufferQueue is empty, it means decryptor can't return ready buffer right
  // now but it can accept more data. The buffer will be returned together with
  // other buffers in a later call to Decrypt. Some of the implementations
  // expect more data to keep themselves running.
  // Once Decrypt is called with EOS buffer, implementation should decrypt all
  // the buffers and return them in one callback. In other words, the total
  // number of output buffers should be the same as number of input buffers.
  // Caller won't call Decrypt again once EOS is pushed.
  using DecryptCB = base::RepeatingCallback<void(bool, BufferQueue)>;

  virtual ~StreamDecryptor() = default;

  virtual void Init(const DecryptCB& decrypt_cb) = 0;

  // Decrypts |buffer| and returns the clear buffers in DecryptCB. Caller must
  // not call Decrypt again until |decrypt_cb| is called. |decrypt_cb| will be
  // called once for each call to Decrypt.
  virtual void Decrypt(scoped_refptr<DecoderBufferBase> buffer) = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_PIPELINE_STREAM_DECRYPTOR_H_
