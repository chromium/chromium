// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READALOUD_DECODER_OPUS_DECODER_HELPER_H_
#define CHROME_SERVICES_READALOUD_DECODER_OPUS_DECODER_HELPER_H_

#include <cstdint>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
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
  ~OpusDecoderHelper();

  OpusDecoderHelper(const OpusDecoderHelper&) = delete;
  OpusDecoderHelper& operator=(const OpusDecoderHelper&) = delete;

  // Asynchronously decodes the compressed Ogg/Opus container bytes, slices the
  // output raw audio into sentence-level segments at indices defined by
  // `sentence_chunk_indices` and `timings`, and returns the segments via the
  // `callback`.
  //
  // Calls the callback with an empty vector if `container_buffer` is empty.
  //
  // The `callback` is guaranteed to be invoked on the same sequence that
  // `DecodeAndSlice` was called on.
  void DecodeAndSlice(
      scoped_refptr<media::DecoderBuffer> container_buffer,
      const std::vector<DecodedAudioSegment::WordTiming>& timings,
      const std::vector<int32_t>& sentence_chunk_indices,
      DecodeCallback callback);

 private:
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace readaloud

#endif  // CHROME_SERVICES_READALOUD_DECODER_OPUS_DECODER_HELPER_H_
