// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_H_
#define COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/media_effects/video_effects_manager_impl.h"
#include "content/public/browser/browser_context.h"
#include "media/capture/mojom/video_effects_manager.mojom-forward.h"
#include "services/video_effects/public/cpp/buildflags.h"

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
#include "components/media_effects/media_effects_model_provider.h"
#include "components/viz/host/gpu_client.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom-forward.h"
#endif

class MediaEffectsService : public KeyedService
#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
    ,
                            public MediaEffectsModelProvider::Observer
#endif
{
 public:
#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  // `model_provider` may be null in case the video effects are not enabled.
  explicit MediaEffectsService(
      PrefService* prefs,
      std::unique_ptr<MediaEffectsModelProvider> model_provider);
#endif

  explicit MediaEffectsService(PrefService* prefs);

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
  // process.
  void BindReadonlyVideoEffectsManager(
      const std::string& device_id,
      mojo::PendingReceiver<media::mojom::ReadonlyVideoEffectsManager>
          effects_manager_receiver);

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
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
  // `BindReadonlyVideoEffectsManager()` instead.
  //
  // Calling this method will launch a new instance of Video Effects Service if
  // it's not already running.
  void BindVideoEffectsProcessor(
      const std::string& device_id,
      mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
          effects_processor_receiver);

  // MediaEffectsModelProvider::Observer:
  void OnBackgroundSegmentationModelUpdated(
      base::optional_ref<const base::FilePath> path) override;
#endif

  VideoEffectsManagerImpl& GetOrCreateVideoEffectsManager(
      const std::string& device_id);

 private:
  void OnLastReceiverDisconnected(const std::string& device_id);

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  // Invoked when the background segmentation model file has been opened.
  void OnBackgroundSegmentationModelOpened(base::File model_file);
#endif

  SEQUENCE_CHECKER(sequence_checker_);

  raw_ptr<PrefService> prefs_;

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  // May be null if the video effects are not enabled:
  std::unique_ptr<MediaEffectsModelProvider> model_provider_;

  // The model file that was most recently opened. The file may be invalid
  // (`.IsValid() == false`) if we have not opened any model file yet.
  base::File latest_segmentation_model_file_;

  std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> gpu_client_ = {
      nullptr, base::OnTaskRunnerDeleter(nullptr)};
#endif

  // Device ID strings mapped to effects manager instances.
  base::flat_map<std::string, std::unique_ptr<VideoEffectsManagerImpl>>
      video_effects_managers_;

  // Must be last:
  base::WeakPtrFactory<MediaEffectsService> weak_factory_{this};
};

#endif  // COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_H_
