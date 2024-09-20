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
#include "components/media_effects/media_effects_model_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_client.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_effects/public/cpp/video_effects_service_host.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"
#include "services/viz/public/mojom/gpu.mojom.h"

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

MediaEffectsService::~MediaEffectsService() {
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
}

void MediaEffectsService::BindVideoEffectsManager(
    const std::string& device_id,
    mojo::PendingReceiver<media::mojom::VideoEffectsManager>
        effects_manager_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto& effects_manager = GetOrCreateVideoEffectsManager(device_id);
  effects_manager.Bind(std::move(effects_manager_receiver));
}

void MediaEffectsService::BindVideoEffectsProcessor(
    const std::string& device_id,
    mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
        effects_processor_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  mojo::PendingRemote<media::mojom::VideoEffectsManager> video_effects_manager;
  BindVideoEffectsManager(
      device_id, video_effects_manager.InitWithNewPipeAndPassReceiver());

  auto* video_effects_service = video_effects::GetVideoEffectsService();
  CHECK(video_effects_service);

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
      device_id, std::move(gpu_remote), std::move(video_effects_manager),
      std::move(effects_processor_receiver));
}

void MediaEffectsService::OnBackgroundSegmentationModelUpdated(
    const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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
          path),
      base::BindOnce(&MediaEffectsService::OnBackgroundSegmentationModelOpened,
                     weak_factory_.GetWeakPtr()));
}

void MediaEffectsService::OnBackgroundSegmentationModelOpened(
    base::File model_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Swap newly opened file with the old one and then close the old one:
  std::swap(latest_segmentation_model_file_, model_file);
  // Closing a file is considered blocking, schedule it in a context where
  // blocking is allowed:
  if (model_file.IsValid()) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::DoNothingWithBoundArgs(std::move(model_file)));
  }

  // Propagate the new file to Video Effects Service if valid:
  if (!latest_segmentation_model_file_.IsValid()) {
    return;
  }

  video_effects::GetVideoEffectsService()->SetBackgroundSegmentationModel(
      latest_segmentation_model_file_.Duplicate());
}

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
