// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_DECODER_OPUS_DECODER_HELPER_H_
#define CHROME_SERVICES_READALOUD_DECODER_OPUS_DECODER_HELPER_H_

#include <cstdint>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"

namespace media {
class DecoderBuffer;
}

namespace readaloud {

class OpusDecoderHelper {
 public:
  using DecodeCallback =
      base::OnceCallback<void(std::vector<scoped_refptr<DecodedAudioSegment>>)>;

  OpusDecoderHelper();
  virtual ~OpusDecoderHelper();

  OpusDecoderHelper(const OpusDecoderHelper&) = delete;
  OpusDecoderHelper& operator=(const OpusDecoderHelper&) = delete;

  // Asynchronously decodes the compressed Ogg/Opus container bytes, slices the
  // output raw audio into word-level segments based on the provided
  // `timings`, and returns the segments via the `callback`.
  //
  // Calls the callback with an empty vector if `container_buffer` is empty.
  //
  // The `callback` is guaranteed to be invoked on the same sequence that
  // `DecodeAndSlice` was called on.
  virtual void DecodeAndSlice(
      scoped_refptr<media::DecoderBuffer> container_buffer,
      const std::vector<DecodedAudioSegment::WordTiming>& timings,
      DecodeCallback callback);

 private:
  // Callback executed on the main sequence thread once the background
  // ThreadPool
  // decoding task has completed. Packages the decoded buffer into a segment and
  // executes the client's callback.
  void OnDecodeFinished(
      const std::vector<DecodedAudioSegment::WordTiming>& timings,
      DecodeCallback callback,
      scoped_refptr<media::AudioBuffer> decoded_buffer);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<OpusDecoderHelper> weak_ptr_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_DECODER_OPUS_DECODER_HELPER_H_
