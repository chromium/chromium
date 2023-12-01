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

namespace {

// Find system default device name if exist.
const std::optional<std::string> GetDefaultMicName(
    const std::vector<media::AudioDeviceDescription>& device_infos) {
  const auto& system_default_device_it = base::ranges::find(
      device_infos, media::AudioDeviceDescription::kDefaultDeviceId,
      &media::AudioDeviceDescription::unique_id);
  return system_default_device_it == device_infos.end()
             ? std::nullopt
             : std::optional<std::string>(
                   system_default_device_it->device_name);
}

}  // namespace

MicCoordinator::MicCoordinator(views::View& parent_view, bool needs_borders)
    : mic_mediator_(
          base::BindRepeating(&MicCoordinator::OnAudioSourceInfosReceived,
                              base::Unretained(this))) {
  auto* mic_view = parent_view.AddChildView(std::make_unique<MediaView>());
  mic_view_tracker_.SetView(mic_view);
  // Safe to use base::Unretained() because `this` owns / outlives
  // `camera_view_tracker_`.
  mic_view_tracker_.SetOnViewIsDeletingCallback(base::BindOnce(
      &MicCoordinator::ResetViewController, base::Unretained(this)));

  // Safe to use base::Unretained() because `this` owns / outlives
  // `mic_view_controller_`.
  mic_view_controller_.emplace(
      *mic_view, needs_borders, combobox_model_,
      base::BindRepeating(&MicCoordinator::OnAudioSourceChanged,
                          base::Unretained(this)));
}

MicCoordinator::~MicCoordinator() = default;

void MicCoordinator::OnAudioSourceInfosReceived(
    const std::vector<media::AudioDeviceDescription>& device_infos) {
  if (!mic_view_controller_.has_value()) {
    return;
  }

  const std::optional<std::string> system_default_device_name =
      GetDefaultMicName(device_infos);

  std::vector<AudioSourceInfo> relevant_device_infos;
  relevant_device_infos.reserve(device_infos.size() -
                                system_default_device_name.has_value());

  for (const auto& device_info : device_infos) {
    if (device_info.unique_id ==
        media::AudioDeviceDescription::kDefaultDeviceId) {
      continue;
    }
    bool is_default =
        system_default_device_name &&
        device_info.device_name == system_default_device_name.value();
    relevant_device_infos.emplace_back(device_info, is_default);
  }

  if (relevant_device_infos.empty()) {
    active_device_id_.clear();
  }
  mic_view_controller_->UpdateAudioSourceInfos(
      std::move(relevant_device_infos));
}

void MicCoordinator::OnAudioSourceChanged(
    std::optional<size_t> selected_index) {
  if (!selected_index.has_value()) {
    return;
  }

  const auto& device_info =
      combobox_model_.GetDeviceInfoAt(selected_index.value());
  if (active_device_id_ == device_info.id) {
    return;
  }

  active_device_id_ = device_info.id;
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
    // TODO(ahmedmoussa): `audio_stream_factory` is to be passed to
    // AudioStreamCoordiantor. Done in the following CL.
  }
}

void MicCoordinator::ResetViewController() {
  mic_view_controller_.reset();
}
