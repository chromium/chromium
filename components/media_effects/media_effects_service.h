// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_H_
#define COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
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
      std::unique_ptr<MediaEffectsModelProvider> model_provider);
#endif

  explicit MediaEffectsService();

  MediaEffectsService(const MediaEffectsService&) = delete;
  MediaEffectsService& operator=(const MediaEffectsService&) = delete;

  MediaEffectsService(MediaEffectsService&&) = delete;
  MediaEffectsService& operator=(MediaEffectsService&&) = delete;

  ~MediaEffectsService() override;

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  // MediaEffectsModelProvider::Observer:
  void OnBackgroundSegmentationModelUpdated(
      base::optional_ref<const base::FilePath> path) override;
#endif

 private:
#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  // Invoked when the background segmentation model file has been opened.
  void OnBackgroundSegmentationModelOpened(base::File model_file);
#endif

  SEQUENCE_CHECKER(sequence_checker_);

#if BUILDFLAG(ENABLE_VIDEO_EFFECTS)
  // May be null if the video effects are not enabled:
  std::unique_ptr<MediaEffectsModelProvider> model_provider_;

  // The model file that was most recently opened. The file may be invalid
  // (`.IsValid() == false`) if we have not opened any model file yet.
  base::File latest_segmentation_model_file_;

  std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> gpu_client_ = {
      nullptr, base::OnTaskRunnerDeleter(nullptr)};
#endif

  // Must be last:
  base::WeakPtrFactory<MediaEffectsService> weak_factory_{this};
};

#endif  // COMPONENTS_MEDIA_EFFECTS_MEDIA_EFFECTS_SERVICE_H_
