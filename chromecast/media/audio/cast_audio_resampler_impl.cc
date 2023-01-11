// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chromecast/media/api/cast_audio_resampler.h"
#include "media/base/sinc_resampler.h"

namespace chromecast {
namespace media {

namespace {

constexpr int kRequestFrames = 128;

class CastAudioResamplerImpl : public CastAudioResampler {
 public:
  CastAudioResamplerImpl(int channel_count,
                         int input_sample_rate,
                         int output_sample_rate)
      : channel_count_(channel_count) {
    DCHECK_GT(channel_count_, 0);
    const double io_sample_rate_ratio =
        static_cast<double>(input_sample_rate) / output_sample_rate;
    resamplers_.reserve(channel_count_);
    buffered_input_.channels.reserve(channel_count_);
    for (int c = 0; c < channel_count_; ++c) {
      resamplers_.push_back(std::make_unique<::media::SincResampler>(
          io_sample_rate_ratio, kRequestFrames,
          base::BindRepeating(&CastAudioResamplerImpl::ReadCallback,
                              base::Unretained(this), c)));
      buffered_input_.channels.push_back(
          std::make_unique<float[]>(kRequestFrames));
    }
  }

  ~CastAudioResamplerImpl() override = default;

  CastAudioResamplerImpl(const CastAudioResamplerImpl&) = delete;
  CastAudioResamplerImpl& operator=(const CastAudioResamplerImpl&) = delete;

 private:
  void ResampleOneChunk(std::vector<float>* output_channels) {
    int output_frame_offset = output_channels[0].size();
    int output_frames = resamplers_[0]->ChunkSize();
    for (int c = 0; c < channel_count_; ++c) {
      output_channels[c].resize(output_frame_offset + output_frames);
      resamplers_[c]->Resample(output_frames,
                               output_channels[c].data() + output_frame_offset);
    }
  }

  void ReadCallback(int channel_index, int frames, float* dest) {
    DCHECK_LE(buffered_input_.frames, frames);
    std::copy_n(buffered_input_.channels[channel_index].get(),
                buffered_input_.frames, dest);

    int frames_left = frames - buffered_input_.frames;
    int dest_offset = buffered_input_.frames;
    if (frames_left) {
      CopyCurrentInputTo(channel_index, frames_left, dest + dest_offset);
    }

    if (channel_index == channel_count_ - 1) {
      buffered_input_.frames = 0;
    }
  }

  void CopyCurrentInputTo(int channel_index, int frames_to_copy, float* dest) {
    DCHECK(current_input_.data);
    DCHECK_LE(current_input_.frame_offset + frames_to_copy,
              current_input_.frames);
    std::copy_n(current_input_.data + channel_index * current_input_.frames +
                    current_input_.frame_offset,
                frames_to_copy, dest);
    if (channel_index == channel_count_ - 1) {
      current_input_.frame_offset += frames_to_copy;
    }
  }

  // CastAudioResampler implementation:
  void Resample(const float* input,
                int num_frames,
                std::vector<float>* output_channels) override {
    current_input_.data = input;
    current_input_.frames = num_frames;
    current_input_.frame_offset = 0;

    while (buffered_input_.frames + current_input_.frames -
               current_input_.frame_offset >=
           kRequestFrames) {
      ResampleOneChunk(output_channels);
    }

    int frames_left = current_input_.frames - current_input_.frame_offset;
    DCHECK_LE(buffered_input_.frames + frames_left, kRequestFrames);
    for (int c = 0; c < channel_count_; ++c) {
      CopyCurrentInputTo(
          c, frames_left,
          buffered_input_.channels[c].get() + buffered_input_.frames);
    }
    buffered_input_.frames += frames_left;

    current_input_.data = nullptr;
    current_input_.frames = 0;
    current_input_.frame_offset = 0;
  }

  void Flush(std::vector<float>* output_channels) override {
    // TODO(kmackay) May need some additional flushing to get out data stored in
    // the SincResamplers.
    if (buffered_input_.frames == 0) {
      return;
    }

    for (int c = 0; c < channel_count_; ++c) {
      std::fill_n(buffered_input_.channels[c].get() + buffered_input_.frames,
                  kRequestFrames - buffered_input_.frames, 0);
    }
    buffered_input_.frames = kRequestFrames;
    while (buffered_input_.frames) {
      ResampleOneChunk(output_channels);
    }

    for (int c = 0; c < channel_count_; ++c) {
      resamplers_[c]->Flush();
    }
  }

  int BufferedInputFrames() const override {
    return buffered_input_.frames +
           std::round(resamplers_[0]->BufferedFrames());
  }

  const int channel_count_;

  std::vector<std::unique_ptr<::media::SincResampler>> resamplers_;

  struct InputBuffer {
    std::vector<std::unique_ptr<float[]>> channels;
    int frames = 0;
  } buffered_input_;

  struct InputData {
    const float* data = nullptr;
    int frames = 0;
    int frame_offset = 0;
  } current_input_;
};

}  // namespace

// static
std::unique_ptr<CastAudioResampler> CastAudioResampler::Create(
    int channel_count,
    int input_sample_rate,
    int output_sample_rate) {
  return std::make_unique<CastAudioResamplerImpl>(
      channel_count, input_sample_rate, output_sample_rate);
}

}  // namespace media
}  // namespace chromecast
