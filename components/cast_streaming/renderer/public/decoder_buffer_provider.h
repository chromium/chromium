// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_DECODER_BUFFER_PROVIDER_H_
#define COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_DECODER_BUFFER_PROVIDER_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace media {
class AudioDecoderConfig;
class DecoderBuffer;
class VideoDecoderConfig;
}

namespace cast_streaming {

template <typename TConfigType>
class DecoderBufferProvider;

using AudioDecoderBufferProvider =
    DecoderBufferProvider<media::AudioDecoderConfig>;
using VideoDecoderBufferProvider =
    DecoderBufferProvider<media::VideoDecoderConfig>;

// This class provides a way for a caller to asynchronously request a new
// buffer, as well as provide information associated with the buffers which it
// returns.
template <typename TConfigType>
class DecoderBufferProvider {
 public:
  using NewBufferCb =
      base::OnceCallback<void(scoped_refptr<media::DecoderBuffer> buffer)>;
  using GetConfigCb = base::OnceCallback<void(TConfigType)>;
  using DeletionCb = base::OnceCallback<void()>;

  virtual ~DecoderBufferProvider() = default;

  // Returns whether this instance is currently valid. Calls are only supported
  // to a valid instance.
  virtual bool IsValid() const = 0;

  // Fetches the current config. |callback| will be called with this config
  // value as long as this instance is valid at time of calling. |callback|
  // must be callable from any thread.
  virtual void GetConfigAsync(GetConfigCb callback) const = 0;

  // Attempts to read the next available buffer. |callback| will be called with
  // this buffer as long as this instance is valid at time of calling.
  // |callback| must be callable from any thread.
  virtual void ReadBufferAsync(NewBufferCb callback) = 0;

  // Sets the callback to be called when this instance becomes invalid.
  // Following this call, no further calls may be made to this instance.
  // |callback| must be callable from any thread.
  virtual void SetInvalidationCallback(DeletionCb callback) = 0;
};

}  // namespace cast_streaming

#endif  // COMPONENTS_CAST_STREAMING_RENDERER_PUBLIC_DECODER_BUFFER_PROVIDER_H_
