// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/decoder/opus_decoder_helper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"

namespace readaloud {

OpusDecoderHelper::OpusDecoderHelper() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

OpusDecoderHelper::~OpusDecoderHelper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void OpusDecoderHelper::DecodeAndSlice(
    scoped_refptr<media::DecoderBuffer> container_buffer,
    const std::vector<DecodedAudioSegment::WordTiming>& timings,
    const std::vector<int32_t>& sentence_chunk_indices,
    DecodeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Handle null or empty input buffers gracefully by returning an empty list
  // of segments on the caller sequence.
  if (!container_buffer || container_buffer->empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<scoped_refptr<DecodedAudioSegment>>()));
    return;
  }

  // TODO(crbug.com/527525634): Implement actual Ogg demuxing, Opus decoding,
  // and slicing. For CL8 skeleton, we return a single dummy DecodedAudioSegment
  // if the input is valid.
  constexpr int kSampleRate = 16000;
  constexpr int kChannels = 1;
  constexpr int kFrames = 160;  // 10ms frame
  scoped_refptr<media::AudioBuffer> dummy_buffer =
      media::AudioBuffer::CreateEmptyBuffer(media::CHANNEL_LAYOUT_MONO,
                                            kChannels, kSampleRate, kFrames,
                                            base::TimeDelta());

  std::vector<scoped_refptr<DecodedAudioSegment>> result;

  result.push_back(base::MakeRefCounted<DecodedAudioSegment>(
      std::move(dummy_buffer), timings));

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

}  // namespace readaloud
