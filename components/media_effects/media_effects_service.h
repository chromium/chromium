// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_H_
#define COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/media_effects/media_effects_model_provider.h"
#include "components/media_effects/video_effects_manager_impl.h"
#include "components/viz/host/gpu_client.h"
#include "content/public/browser/browser_context.h"
#include "media/capture/mojom/video_effects_manager.mojom-forward.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom-forward.h"

class MediaEffectsService : public KeyedService,
                            public MediaEffectsModelProvider::Observer {
 public:
  // `model_provider` may be null in case the video effects are not enabled.
  explicit MediaEffectsService(
      PrefService* prefs,
      std::unique_ptr<MediaEffectsModelProvider> model_provider);

  MediaEffectsService(const MediaEffectsService&) = delete;
  MediaEffectsService& operator=(const MediaEffectsService&) = delete;

  MediaEffectsService(MediaEffectsService&&) = delete;
  MediaEffectsService& operator=(MediaEffectsService&&) = delete;

  ~MediaEffectsService() override;

  // Connects a `VideoEffectsManagerImpl` to the provided
  // `effects_manager_receiver`. If the keyd profile already has a manager for
  // the passed `device_id`, then it will be used. Otherwise, a new manager will
  // be created.
  //
  // The device id must be the raw string from
  // `media::mojom::VideoCaptureDeviceDescriptor::device_id`.
  //
  // Note that this API only allows interacting with the manager via mojo in
  // order to support communication with the VideoCaptureService in a different
  // process. The usages in Browser UI could potentially directly interact with
  // a manager instance in order to avoid the mojo overhead, interactions
  // are expected to be very low frequency and this approach is worth that
  // tradeoff given the benefits:
  //   * A consistent interaction mechanism for both in-process and
  //     out-of-process clients
  //   * Automatic cleanup when all remotes are disconnected
  void BindVideoEffectsManager(
      const std::string& device_id,
      mojo::PendingReceiver<media::mojom::VideoEffectsManager>
          effects_manager_receiver);

  // Connects a `VideoEffectsManagerImpl` to the provided
  // `effects_processor_receiver`. If the keyed profile already has a manager
  // for the passed `device_id`, then it will be used. Otherwise, a new manager
  // will be created.
  //
  // The device id must be the raw string from
  // `media::mojom::VideoCaptureDeviceDescriptor::device_id`.
  //
  // The manager remote will be sent to the Video Effects Service, where
  // it will be used to subscribe to the effects configuration. The passed in
  // pending receiver is going to be used to create a Video Effects Processor
  // in the Video Effects Service.
  //
  // Note that this API does not expose the `VideoEffectsManagerImpl` in any
  // way. If you need to interact with the manager, call
  // `BindVideoEffectsManager()` instead.
  //
  // Calling this method will launch a new instance of Video Effects Service if
  // it's not already running.
  void BindVideoEffectsProcessor(
      const std::string& device_id,
      mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
          effects_processor_receiver);

  // MediaEffectsModelProvider::Observer:
  void OnBackgroundSegmentationModelUpdated(
      const base::FilePath& path) override;

 private:
  VideoEffectsManagerImpl& GetOrCreateVideoEffectsManager(
      const std::string& device_id);

  void OnLastReceiverDisconnected(const std::string& device_id);

  // Invoked when the background segmentation model file has been opened.
  void OnBackgroundSegmentationModelOpened(base::File model_file);

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<PrefService> prefs_;

  // May be null if the video effects are not enabled:
  std::unique_ptr<MediaEffectsModelProvider> model_provider_;

  // The model file that was most recently opened. The file may be invalid
  // (`.IsValid() == false`) if we have not opened any model file yet.
  base::File latest_segmentation_model_file_;

  // Device ID strings mapped to effects manager instances.
  base::flat_map<std::string, std::unique_ptr<VideoEffectsManagerImpl>>
      video_effects_managers_;

  std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> gpu_client_;

  // Must be last:
  base::WeakPtrFactory<MediaEffectsService> weak_factory_{this};
};

#endif  // COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_H_
