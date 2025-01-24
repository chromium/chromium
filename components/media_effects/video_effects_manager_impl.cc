// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/video_effects_manager_impl.h"

VideoEffectsManagerImpl::VideoEffectsManagerImpl(
    PrefService* pref_service,
    base::OnceClosure last_receiver_disconnected_handler)
    : pref_service_(pref_service),
      last_receiver_disconnected_handler_(
          std::move(last_receiver_disconnected_handler)) {
  configuration_ = media::mojom::VideoEffectsConfiguration::New();
  receivers_.set_disconnect_handler(
      base::BindRepeating(&VideoEffectsManagerImpl::OnReceiverDisconnected,
                          base::Unretained(this)));
}

VideoEffectsManagerImpl::~VideoEffectsManagerImpl() = default;

void VideoEffectsManagerImpl::Bind(
    mojo::PendingReceiver<media::mojom::ReadonlyVideoEffectsManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void VideoEffectsManagerImpl::SetConfiguration(
    media::mojom::VideoEffectsConfigurationPtr configuration) {
  configuration_ = configuration->Clone();
  for (const auto& observer : observers_) {
    observer->OnConfigurationChanged(configuration_->Clone());
  }
}

void VideoEffectsManagerImpl::GetConfiguration(
    GetConfigurationCallback callback) {
  std::move(callback).Run(configuration_->Clone());
}

void VideoEffectsManagerImpl::AddObserver(
    mojo::PendingRemote<media::mojom::VideoEffectsConfigurationObserver>
        observer) {
  auto id = observers_.Add(std::move(observer));
  // Notify new observer of current config state.
  observers_.Get(id)->OnConfigurationChanged(configuration_->Clone());
}

void VideoEffectsManagerImpl::OnReceiverDisconnected() {
  if (!receivers_.empty()) {
    return;
  }

  std::move(last_receiver_disconnected_handler_).Run();
}
