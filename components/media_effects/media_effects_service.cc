// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/media_effects_service.h"

#include <optional>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "content/public/browser/browser_thread.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_effects/public/cpp/buildflags.h"

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
#include "components/media_effects/media_effects_model_provider.h"
#include "content/public/browser/gpu_client.h"
#include "services/video_effects/public/cpp/video_effects_service_host.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"
#endif

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
MediaEffectsService::MediaEffectsService(
    PrefService* prefs,
    std::unique_ptr<MediaEffectsModelProvider> model_provider)
    : prefs_(prefs),
      model_provider_(std::move(model_provider)),
      gpu_client_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
  if (model_provider_) {
    model_provider_->AddObserver(this);
  }
}
#endif

MediaEffectsService::MediaEffectsService(PrefService* prefs) : prefs_(prefs) {}

MediaEffectsService::~MediaEffectsService() {
#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  if (model_provider_) {
    model_provider_->RemoveObserver(this);
  }

  if (latest_segmentation_model_file_.IsValid()) {
    // Closing a file is considered blocking, schedule it in a context where
    // blocking is allowed:
    base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                               base::DoNothingWithBoundArgs(
                                   std::move(latest_segmentation_model_file_)));
  }
#endif
}

void MediaEffectsService::BindReadonlyVideoEffectsManager(
    const std::string& device_id,
    mojo::PendingReceiver<media::mojom::ReadonlyVideoEffectsManager>
        effects_manager_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& effects_manager = GetOrCreateVideoEffectsManager(device_id);
  effects_manager.Bind(std::move(effects_manager_receiver));
}

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
void MediaEffectsService::BindVideoEffectsProcessor(
    const std::string& device_id,
    mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
        effects_processor_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::PendingRemote<media::mojom::ReadonlyVideoEffectsManager>
      readonly_video_effects_manager;
  BindReadonlyVideoEffectsManager(
      device_id,
      readonly_video_effects_manager.InitWithNewPipeAndPassReceiver());

  auto* video_effects_service = video_effects::GetVideoEffectsService();
  CHECK(video_effects_service);

  // The `video_effects_service` is reset if it is idle for more than 5 seconds.
  // Re-send the model in case that has happened.
  if (latest_segmentation_model_file_.IsValid()) {
    video_effects_service->SetBackgroundSegmentationModel(
        latest_segmentation_model_file_.Duplicate());
  }

  mojo::PendingRemote<viz::mojom::Gpu> gpu_remote;
  mojo::PendingReceiver<viz::mojom::Gpu> gpu_receiver =
      gpu_remote.InitWithNewPipeAndPassReceiver();

  if (!gpu_client_) {
    gpu_client_ = content::CreateGpuClient(std::move(gpu_receiver));
  } else {
    auto task_runner = content::GetUIThreadTaskRunner({});
    task_runner->PostTask(FROM_HERE, base::BindOnce(&viz::GpuClient::Add,
                                                    gpu_client_->GetWeakPtr(),
                                                    std::move(gpu_receiver)));
  }

  video_effects_service->CreateEffectsProcessor(
      device_id, std::move(gpu_remote),
      std::move(readonly_video_effects_manager),
      std::move(effects_processor_receiver));
}

void MediaEffectsService::OnBackgroundSegmentationModelUpdated(
    base::optional_ref<const base::FilePath> path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!path.has_value()) {
    // We have not received a valid path, so there is nothing to open.
    // Just pass an invalid `base::File`, it will be handled by lower
    // layers.
    OnBackgroundSegmentationModelOpened(base::File());
    return;
  }

  // We have received new path to the model, let's open it and inform the Video
  // Effects Service about it. Opening a file is considered blocking, schedule
  // it in a context where blocking is allowed:
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](const base::FilePath& model_path) {
            if (!base::PathExists(model_path)) {
              return base::File();
            }

            base::File model(model_path, base::File::Flags::FLAG_OPEN |
                                             base::File::Flags::FLAG_READ);

            return model;
          },
          *path),
      base::BindOnce(&MediaEffectsService::OnBackgroundSegmentationModelOpened,
                     weak_factory_.GetWeakPtr()));
}

void MediaEffectsService::OnBackgroundSegmentationModelOpened(
    base::File model_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  video_effects::GetVideoEffectsService()->SetBackgroundSegmentationModel(
      model_file.Duplicate());

  // Swap newly opened file with the old one and then close the old one:
  std::swap(latest_segmentation_model_file_, model_file);
  // Closing a file is considered blocking, schedule it in a context where
  // blocking is allowed:
  if (model_file.IsValid()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::DoNothingWithBoundArgs(std::move(model_file)));
  }
}
#endif

VideoEffectsManagerImpl& MediaEffectsService::GetOrCreateVideoEffectsManager(
    const std::string& device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (auto effects_manager = video_effects_managers_.find(device_id);
      effects_manager != video_effects_managers_.end()) {
    return *effects_manager->second;
  }

  // base::Unretained is safe here because `this` owns the
  // `VideoEffectsManagerImpl` that would call this callback.
  auto [effects_manager, inserted] = video_effects_managers_.emplace(
      device_id,
      std::make_unique<VideoEffectsManagerImpl>(
          prefs_,
          base::BindOnce(&MediaEffectsService::OnLastReceiverDisconnected,
                         base::Unretained(this), device_id)));
  CHECK(inserted);
  return *effects_manager->second;
}

void MediaEffectsService::OnLastReceiverDisconnected(
    const std::string& device_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  video_effects_managers_.erase(device_id);
}
