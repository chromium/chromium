// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_MEDIA_EXTERNAL_AUDIO_DECODER_H_
#define CHROMECAST_PUBLIC_MEDIA_EXTERNAL_AUDIO_DECODER_H_

#include <cstddef>

#include "cast_decoder_buffer.h"
#include "chromecast_export.h"
#include "decoder_config.h"

namespace chromecast {
namespace media {

// The external audio decoder is an optional dynamically-loaded shared library.
// The library is loaded as "libcast_external_decoder.so" from the cast_shell
// shared library search path. See below for the functions that the library
// must export.

// Interface for additional audio decoder functionality. ExternalAudioDecoder
// always decodes to planar float format.
class ExternalAudioDecoder {
 public:
  class Delegate {
   public:
    // Allocates a buffer to write decoded audio into. The caller may write
    // up to |bytes| bytes of decoded audio into the returned buffer, in the
    // desired output format. To avoid memory leaks, OnDecodedBuffer() must be
    // called for every allocated buffer. It is an error to call AllocateBuffer
    // twice in a row without calling OnDecodedBuffer() in between.
    virtual void* AllocateBuffer(size_t bytes) = 0;

    // Informs the delegate that |decoded_size_bytes| of decoded audio data have
    // been written into the buffer most recently returned by AllocateBuffer().
    // |decoded_size_bytes| may be 0. |config| is the config of the decoded
    // audio. It is assumed that the config is the same for all
    // OnDecodedBuffer() calls that occur within a single call to
    // ExternalAudioDecoder::Decode(), but the config may change between calls
    // to Decode(). Decoded audio must be in planar float format.
    virtual void OnDecodedBuffer(size_t decoded_size_bytes,
                                 const AudioConfig& config) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Returns the expected number of channels in the decoded audio. This is
  // called immediately after construction. Returns 0 if the number of channels
  // is not known.
  virtual int GetNumOutputChannels() = 0;

  // Decodes an audio buffer. The implementation of this method should call
  // the delegate's AllocateBuffer() method to allocate a buffer, write some
  // decoded data to the buffer, and then call the delegate's OnDecodedBuffer()
  // method. This may be done 0 or more times per call to Decode(). All delegate
  // methods must be called synchronously from within Decode(). Returns |false|
  // if an error occurred while decoding.
  virtual bool Decode(const CastDecoderBuffer& buffer) = 0;

 protected:
  virtual ~ExternalAudioDecoder() = default;
};

// The external decoder library must export the following functions:

// Returns |true| if |config| is supported by the external decoder library,
// |false| otherwise. An ExternalAudioDecoder instance will be created for any
// stream where |true| is returned for the config.
extern "C" CHROMECAST_EXPORT bool ExternalAudioDecoder_IsSupportedConfig(
    const AudioConfig& config);

// Creates an external audio decoder. The |delegate| is guaranteed to outlive
// the returned decoder instance. The returned decoder will be deleted by
// calling ExternalAudioDecoder_DeleteDecoder().
extern "C" CHROMECAST_EXPORT ExternalAudioDecoder*
ExternalAudioDecoder_CreateDecoder(ExternalAudioDecoder::Delegate* delegate,
                                   const AudioConfig& config);

// Deletes an external audio decoder.
extern "C" CHROMECAST_EXPORT void ExternalAudioDecoder_DeleteDecoder(
    ExternalAudioDecoder* decoder);

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_MEDIA_EXTERNAL_AUDIO_DECODER_H_
