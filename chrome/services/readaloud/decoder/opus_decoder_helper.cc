// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/readaloud/decoder/opus_decoder_helper.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/services/readaloud/decoded_audio_segment.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/decoder_buffer.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_FFMPEG)
#include "media/filters/audio_file_reader.h"
#include "media/filters/in_memory_url_protocol.h"
#endif

namespace readaloud {

namespace {

scoped_refptr<media::AudioBuffer> DecodeOnBackgroundThread(
    scoped_refptr<media::DecoderBuffer> container_buffer) {
#if BUILDFLAG(ENABLE_FFMPEG)
  // InMemoryUrlProtocol wraps the input DecoderBuffer so that FFmpeg can read
  // from memory as a stream. This is needed because FFmpeg URL protocols
  // usually operate on files or sockets.
  media::InMemoryUrlProtocol protocol(*container_buffer, /*streaming=*/false);
  media::AudioFileReader reader(&protocol);
  if (!reader.Open()) {
    DLOG(WARNING) << "AudioFileReader failed to open";
    return nullptr;
  }

  // AudioFileReader decodes the audio data packet-by-packet, returning a list
  // of individual AudioBus objects. Since we need a single continuous audio
  // stream to perform sentence slicing and timestamp shifting relative to the
  // start, we concatenate all the chunks into a single output AudioBus.
  std::vector<std::unique_ptr<media::AudioBus>> decoded_buses;
  int total_frames = reader.Read(&decoded_buses);
  if (total_frames <= 0 || decoded_buses.empty()) {
    return nullptr;
  }

  int channels = reader.channels();
  int sample_rate = reader.sample_rate();

  std::unique_ptr<media::AudioBus> output_bus =
      media::AudioBus::Create(channels, total_frames);
  int write_offset = 0;
  for (const auto& bus : decoded_buses) {
    bus->CopyPartialFramesTo(0, bus->frames(), write_offset, output_bus.get());
    write_offset += bus->frames();
  }

  return media::AudioBuffer::CopyFrom(sample_rate, base::TimeDelta(),
                                      output_bus.get());
#else
  // Fallback when FFmpeg is disabled (e.g., in iOS or special non-Blink builds)
  return nullptr;
#endif  // BUILDFLAG(ENABLE_FFMPEG)
}

}  // namespace

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

  // Offload demuxing and decoding to Chromium's background ThreadPool to avoid
  // blocking the main Mojo/IPC thread. This ensures the main thread remains
  // fully responsive to user input events (like Pause or Seek) during decoding.
  // Once the ThreadPool worker completes, the results are posted back to the
  // main thread via the OnDecodeFinished callback.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&DecodeOnBackgroundThread, std::move(container_buffer)),
      base::BindOnce(&OpusDecoderHelper::OnDecodeFinished,
                     weak_ptr_factory_.GetWeakPtr(), timings,
                     std::move(callback)));
}

void OpusDecoderHelper::OnDecodeFinished(
    const std::vector<DecodedAudioSegment::WordTiming>& timings,
    DecodeCallback callback,
    scoped_refptr<media::AudioBuffer> decoded_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/527525634): Implement sentence slicing logic to divide the
  // decoded buffer into multiple DecodedAudioSegments based on sentence chunk
  // indices and word boundaries. For now, package the full buffer into a single
  // DecodedAudioSegment.
  std::vector<scoped_refptr<DecodedAudioSegment>> result;
  if (decoded_buffer) {
    result.push_back(base::MakeRefCounted<DecodedAudioSegment>(
        std::move(decoded_buffer), timings));
  }

  std::move(callback).Run(std::move(result));
}

}  // namespace readaloud
