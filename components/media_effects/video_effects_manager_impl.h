// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_VIDEO_EFFECTS_MANAGER_IMPL_H_
#define COMPONENTS_MEDIA_EFFECTS_VIDEO_EFFECTS_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class PrefService;

class VideoEffectsManagerImpl : public media::mojom::VideoEffectsManager {
 public:
  VideoEffectsManagerImpl(PrefService* pref_service,
                          base::OnceClosure last_receiver_disconnected_handler);

  VideoEffectsManagerImpl(const VideoEffectsManagerImpl&) = delete;
  VideoEffectsManagerImpl& operator=(const VideoEffectsManagerImpl&) = delete;

  ~VideoEffectsManagerImpl() override;

  void Bind(mojo::PendingReceiver<media::mojom::VideoEffectsManager> receiver);

  // media::mojom::VideoEffectsManager overrides
  void GetConfiguration(GetConfigurationCallback callback) override;
  void SetConfiguration(
      media::mojom::VideoEffectsConfigurationPtr configuration,
      SetConfigurationCallback callback) override;
  void AddObserver(
      mojo::PendingRemote<media::mojom::VideoEffectsConfigurationObserver>
          observer) override;

 private:
  void OnReceiverDisconnected();

  raw_ptr<PrefService> pref_service_;
  base::OnceClosure last_receiver_disconnected_handler_;
  mojo::ReceiverSet<media::mojom::VideoEffectsManager> receivers_;

  media::mojom::VideoEffectsConfigurationPtr configuration_;
  mojo::RemoteSet<media::mojom::VideoEffectsConfigurationObserver> observers_;
};

#endif  // COMPONENTS_MEDIA_EFFECTS_VIDEO_EFFECTS_MANAGER_IMPL_H_
