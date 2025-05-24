// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/mic_preview/audio_stream_coordinator.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "chrome/browser/ui/views/media_preview/mic_preview/audio_stream_view.h"
#include "components/permissions/permission_hats_trigger_helper.h"
#include "components/permissions/permission_request.h"

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

AudioStreamCoordinator::AudioStreamCoordinator(
    views::View& parent_view,
    media_preview_metrics::Context metrics_context)
    : metrics_context_(metrics_context),
      audio_stream_construction_time_(base::TimeTicks::Now()) {
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

  audio_stream_request_time_ = base::TimeTicks::Now();

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

  float max_audio_value = std::ranges::max(audio_bus->channel_span(0));
  last_audio_level_ = GetRolledAverageValue(max_audio_value, last_audio_level_);

  if (auto* view = GetAudioStreamView(); view) {
    view->ScheduleAudioStreamPaint(last_audio_level_);
  }

  if (!audio_stream_start_time_) {
    OnReceivedFirstAudioSample();
  }
}

void AudioStreamCoordinator::OnReceivedFirstAudioSample() {
  audio_stream_start_time_ = base::TimeTicks::Now();

  CHECK(audio_stream_request_time_);
  const auto preview_delay_time =
      *audio_stream_start_time_ - *audio_stream_request_time_;

  // We now know that audio levels are being shown, so the preview was visible.
  // We also now know how long it took for the preview to show.
  if (metrics_context_.request) {
    auto preview_params =
        metrics_context_.request->get_preview_parameters().value_or(
            permissions::PermissionHatsTriggerHelper::
                PreviewParametersForHats{});
    preview_params.MergeParameters(
        permissions::PermissionHatsTriggerHelper::PreviewParametersForHats(
            /*was_visible=*/true, /*dropdown_was_interacted=*/false,
            /*was_prompt_combined=*/metrics_context_.prompt_type ==
                media_preview_metrics::PromptType::kCombined,
            /*time_to_decision=*/{},
            /*time_to_visible=*/preview_delay_time));
    metrics_context_.request->set_preview_parameters(preview_params);
  }

  audio_stream_request_time_.reset();
}

void AudioStreamCoordinator::Stop() {
  audio_stream_start_time_ = std::nullopt;

  if (audio_capturing_callback_) {
    audio_capturing_callback_->Stop();
    audio_capturing_callback_.reset();
  }
  if (auto* view = GetAudioStreamView(); view) {
    view->Clear();
  }
  last_audio_level_ = 0;
}

void AudioStreamCoordinator::OnClosing() {
  // We now know that the decision was made by the user.
  if (metrics_context_.request) {
    auto preview_params =
        metrics_context_.request->get_preview_parameters().value_or(
            permissions::PermissionHatsTriggerHelper::
                PreviewParametersForHats{});
    preview_params.MergeParameters(
        permissions::PermissionHatsTriggerHelper::PreviewParametersForHats(
            /*was_visible=*/false, /*dropdown_was_interacted=*/false,
            /*was_prompt_combined=*/metrics_context_.prompt_type ==
                media_preview_metrics::PromptType::kCombined,
            /*time_to_decision=*/base::TimeTicks::Now() -
                audio_stream_construction_time_,
            /*time_to_visible=*/{}));
    metrics_context_.request->set_preview_parameters(preview_params);
  }
}

AudioStreamView* AudioStreamCoordinator::GetAudioStreamView() {
  auto* view = audio_stream_view_tracker_.view();
  return view ? static_cast<AudioStreamView*>(view) : nullptr;
}
