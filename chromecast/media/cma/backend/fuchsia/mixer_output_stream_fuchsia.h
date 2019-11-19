// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_OUTPUT_STREAM_FUCHSIA_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_OUTPUT_STREAM_FUCHSIA_H_

#include <fuchsia/media/cpp/fidl.h>

#include "base/memory/shared_memory_mapping.h"
#include "base/time/time.h"
#include "chromecast/public/media/mixer_output_stream.h"

namespace chromecast {
namespace media {

// MixerOutputStream implementation for Fuchsia.
class MixerOutputStreamFuchsia : public MixerOutputStream {
 public:
  MixerOutputStreamFuchsia();
  ~MixerOutputStreamFuchsia() override;

  // MixerOutputStream implementation:
  bool Start(int requested_sample_rate, int channels) override;
  int GetNumChannels() override;
  int GetSampleRate() override;
  MediaPipelineBackend::AudioDecoder::RenderingDelay GetRenderingDelay()
      override;
  int OptimalWriteFramesCount() override;
  bool Write(const float* data,
             int data_size,
             bool* out_playback_interrupted) override;
  void Stop() override;

 private:
  size_t GetMinBufferSize();
  bool InitializePayloadBuffer();

  base::TimeTicks GetCurrentStreamTime();

  // Event handlers for |audio_renderer_|.
  void OnRendererError(zx_status_t status);
  void OnMinLeadTimeChanged(int64_t min_lead_time);

  int sample_rate_ = 0;
  int channels_ = 0;

  // Value returned by OptimalWriteFramesCount().
  int target_packet_size_ = 0;

  // Audio renderer connection.
  fuchsia::media::AudioRendererPtr audio_renderer_;

  base::WritableSharedMemoryMapping payload_buffer_;
  size_t payload_buffer_pos_ = 0;

  // Set only while stream is playing.
  base::TimeTicks reference_time_;

  int64_t stream_position_samples_ = 0;

  // Current min lead time for the stream. This value is updated by
  // AudioRenderer::OnMinLeadTimeChanged event. Assume 50ms until we get the
  // first OnMinLeadTimeChanged event.
  base::TimeDelta min_lead_time_ = base::TimeDelta::FromMilliseconds(50);

  DISALLOW_COPY_AND_ASSIGN(MixerOutputStreamFuchsia);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_AUDIO_OUTPUT_STREAM_FUCHSIA_H_
