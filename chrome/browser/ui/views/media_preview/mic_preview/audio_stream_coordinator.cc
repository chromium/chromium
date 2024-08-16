// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/views/media_preview/mic_preview/audio_stream_coordinator.h"

#include <memory>

#include "base/task/bind_post_task.h"
#include "chrome/browser/ui/views/media_preview/mic_preview/audio_stream_view.h"

namespace {

// Maximum acceptable audio value.
constexpr float kMaxAudioValue = 1.f;
// Number of audio buffers to receive per second.
constexpr int kNumberOfBuffersPerSecond = 20;
// A multiplier used to scale up audio values.
constexpr float kAudioValueScaler = 7.5;
// The number of values considered while computing the average.
// This value is based on kRolledAverageWindow of 0.2 second.
// Computed as `kNumberOfBuffersPerSecond` * `kRolledAverageWindow`.
constexpr int kRolledAverageSize = 4;

float GetRolledAverageValue(float current_audio_value,
                            float previous_audio_level) {
  float scaled_audio_value =
      std::min(kMaxAudioValue, current_audio_value * kAudioValueScaler);

  return (previous_audio_level * kRolledAverageSize + scaled_audio_value) /
         (kRolledAverageSize + 1);
}

}  // namespace

AudioStreamCoordinator::AudioStreamCoordinator(views::View& parent_view) {
  auto* audio_stream_view =
      parent_view.AddChildView(std::make_unique<AudioStreamView>());
  audio_stream_view_tracker_.SetView(audio_stream_view);
}

AudioStreamCoordinator::~AudioStreamCoordinator() {
  Stop();
}

void AudioStreamCoordinator::ConnectToDevice(
    mojo::PendingRemote<media::mojom::AudioStreamFactory> audio_stream_factory,
    const std::string& device_id,
    int sample_rate) {
  Stop();

  const media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::ChannelLayoutConfig::Mono(), sample_rate,
      sample_rate / kNumberOfBuffersPerSecond);

  audio_capturing_callback_ = std::make_unique<capture_mode::AudioCapturer>(
      device_id, std::move(audio_stream_factory), params,
      base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&AudioStreamCoordinator::OnAudioCaptured,
                              weak_factory_.GetWeakPtr())));

  audio_capturing_callback_->Start();
}

void AudioStreamCoordinator::OnAudioCaptured(
    std::unique_ptr<media::AudioBus> audio_bus,
    base::TimeTicks capture_time) {
  if (!audio_bus || !audio_bus->channels()) {
    return;
  }

  if (audio_bus_received_callback_for_test_) {
    audio_bus_received_callback_for_test_.Run();
  }

  float max_audio_value = 0;
  const float* channel = audio_bus->channel(0);
  for (int frame_index = 0; frame_index < audio_bus->frames(); frame_index++) {
    max_audio_value = std::max(max_audio_value, channel[frame_index]);
  }
  last_audio_level_ = GetRolledAverageValue(max_audio_value, last_audio_level_);

  if (auto* view = GetAudioStreamView(); view) {
    view->ScheduleAudioStreamPaint(last_audio_level_);
  }
}

void AudioStreamCoordinator::Stop() {
  if (audio_capturing_callback_) {
    audio_capturing_callback_->Stop();
    audio_capturing_callback_.reset();
  }
  if (auto* view = GetAudioStreamView(); view) {
    view->Clear();
  }
  last_audio_level_ = 0;
}

AudioStreamView* AudioStreamCoordinator::GetAudioStreamView() {
  auto* view = audio_stream_view_tracker_.view();
  return view ? static_cast<AudioStreamView*>(view) : nullptr;
}
