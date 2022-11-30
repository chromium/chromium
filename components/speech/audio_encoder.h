// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPEECH_AUDIO_ENCODER_H_
#define COMPONENTS_SPEECH_AUDIO_ENCODER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/speech/audio_buffer.h"
#include "third_party/flac/include/FLAC/stream_encoder.h"

class AudioChunk;

class FlacStreamEncoderDeleter {
 public:
  void operator()(FLAC__StreamEncoder* ptr) {
    FLAC__stream_encoder_delete(ptr);
  }
};

// Provides a simple interface to encode raw audio using FLAC codec.
class AudioEncoder {
 public:
  AudioEncoder(int sampling_rate, int bits_per_sample);

  AudioEncoder(const AudioEncoder&) = delete;
  AudioEncoder& operator=(const AudioEncoder&) = delete;

  ~AudioEncoder();

  // Encodes |raw audio| to the internal buffer. Use
  // |GetEncodedDataAndClear| to read the result after this call or when
  // audio capture completes.
  void Encode(const AudioChunk& raw_audio);

  // Finish encoding and flush any pending encoded bits out.
  void Flush();

  // Merges, retrieves and clears all the accumulated encoded audio chunks.
  scoped_refptr<AudioChunk> GetEncodedDataAndClear();

  std::string GetMimeType();
  int GetBitsPerSample();

 private:
  AudioBuffer encoded_audio_buffer_;

  std::unique_ptr<FLAC__StreamEncoder, FlacStreamEncoderDeleter> encoder_;
  bool is_encoder_initialized_;
};

#endif  // COMPONENTS_SPEECH_AUDIO_ENCODER_H_
