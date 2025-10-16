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
    std::unique_ptr<MediaEffectsModelProvider> model_provider)
    : model_provider_(std::move(model_provider)),
      gpu_client_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {
  if (model_provider_) {
    model_provider_->AddObserver(this);
  }
}
#endif

MediaEffectsService::MediaEffectsService() = default;

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

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)

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
