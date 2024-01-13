// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/mic_preview/mic_coordinator.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_parameters.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

MicCoordinator::MicCoordinator(views::View& parent_view,
                               bool needs_borders,
                               const std::vector<std::string>& eligible_mic_ids)
    : mic_mediator_(
          base::BindRepeating(&MicCoordinator::OnAudioSourceInfosReceived,
                              base::Unretained(this))),
      combobox_model_({}),
      eligible_mic_ids_(eligible_mic_ids) {
  auto* mic_view = parent_view.AddChildView(std::make_unique<MediaView>());
  mic_view_tracker_.SetView(mic_view);
  // Safe to use base::Unretained() because `this` owns / outlives
  // `camera_view_tracker_`.
  mic_view_tracker_.SetIsDeletingCallback(base::BindOnce(
      &MicCoordinator::ResetViewController, base::Unretained(this)));

  // Safe to use base::Unretained() because `this` owns / outlives
  // `mic_view_controller_`.
  mic_view_controller_.emplace(
      *mic_view, needs_borders, combobox_model_,
      base::BindRepeating(&MicCoordinator::OnAudioSourceChanged,
                          base::Unretained(this)));

  audio_stream_coordinator_.emplace(
      mic_view_controller_->GetLiveFeedContainer());
}

MicCoordinator::~MicCoordinator() = default;

void MicCoordinator::OnAudioSourceInfosReceived(
    const std::vector<media::AudioDeviceDescription>& device_infos) {
  if (!mic_view_controller_.has_value()) {
    return;
  }

  eligible_device_infos_.clear();

  for (const auto& device_info : device_infos) {
    if (device_info.unique_id ==
        media::AudioDeviceDescription::kDefaultDeviceId) {
      continue;
    }
    if (!eligible_mic_ids_.empty() &&
        !eligible_mic_ids_.contains(device_info.unique_id)) {
      continue;
    }
    eligible_device_infos_.emplace_back(device_info);
  }

  if (eligible_device_infos_.empty()) {
    active_device_id_.clear();
    audio_stream_coordinator_->Stop();
  }
  mic_view_controller_->UpdateAudioSourceInfos(eligible_device_infos_);
}

void MicCoordinator::OnAudioSourceChanged(
    std::optional<size_t> selected_index) {
  if (!selected_index.has_value()) {
    return;
  }

  const auto& device_info = eligible_device_infos_.at(selected_index.value());
  if (active_device_id_ == device_info.unique_id) {
    return;
  }

  active_device_id_ = device_info.unique_id;
  mic_mediator_.GetAudioInputDeviceFormats(
      active_device_id_,
      base::BindOnce(&MicCoordinator::ConnectAudioStream,
                     base::Unretained(this), active_device_id_));
}

void MicCoordinator::ConnectAudioStream(
    const std::string& device_id,
    const std::optional<media::AudioParameters>& device_params) {
  if (device_params.has_value()) {
    mojo::PendingRemote<media::mojom::AudioStreamFactory> audio_stream_factory;
    mic_mediator_.BindAudioStreamFactory(
        audio_stream_factory.InitWithNewPipeAndPassReceiver());
    audio_stream_coordinator_->ConnectToDevice(std::move(audio_stream_factory),
                                               device_id,
                                               device_params->sample_rate());
  }
}

void MicCoordinator::ResetViewController() {
  mic_view_controller_.reset();
}
