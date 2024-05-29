// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_effects/media_effects_service.h"

#include <optional>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_client.h"
#include "content/public/browser/service_process_host.h"
#include "media/capture/mojom/video_effects_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/video_effects/public/mojom/video_effects_processor.mojom.h"
#include "services/video_effects/public/mojom/video_effects_service.mojom.h"
#include "services/viz/public/mojom/gpu.mojom.h"

namespace {

static mojo::Remote<video_effects::mojom::VideoEffectsService>*
    g_service_remote = nullptr;

video_effects::mojom::VideoEffectsService* GetVideoEffectsService() {
  if (!g_service_remote) {
    g_service_remote =
        new mojo::Remote<video_effects::mojom::VideoEffectsService>();
  }

  if (!g_service_remote->is_bound()) {
    content::ServiceProcessHost::Launch(
        g_service_remote->BindNewPipeAndPassReceiver(),
        content::ServiceProcessHost::Options()
            .WithDisplayName("Video Effects Service")
            .Pass());

    g_service_remote->reset_on_disconnect();
    g_service_remote->reset_on_idle_timeout(base::Seconds(5));
  }

  return g_service_remote->get();
}

}  // namespace

base::AutoReset<mojo::Remote<video_effects::mojom::VideoEffectsService>*>
SetVideoEffectsServiceRemoteForTesting(
    mojo::Remote<video_effects::mojom::VideoEffectsService>* service_override) {
  return base::AutoReset<
      mojo::Remote<video_effects::mojom::VideoEffectsService>*>(
      &g_service_remote, service_override);
}

MediaEffectsService::MediaEffectsService(PrefService* prefs)
    : prefs_(prefs), gpu_client_(nullptr, base::OnTaskRunnerDeleter(nullptr)) {}

MediaEffectsService::~MediaEffectsService() = default;

void MediaEffectsService::BindVideoEffectsManager(
    const std::string& device_id,
    mojo::PendingReceiver<media::mojom::VideoEffectsManager>
        effects_manager_receiver) {
  auto& effects_manager = GetOrCreateVideoEffectsManager(device_id);
  effects_manager.Bind(std::move(effects_manager_receiver));
}

void MediaEffectsService::BindVideoEffectsProcessor(
    const std::string& device_id,
    mojo::PendingReceiver<video_effects::mojom::VideoEffectsProcessor>
        effects_processor_receiver) {
  mojo::PendingRemote<media::mojom::VideoEffectsManager> video_effects_manager;
  BindVideoEffectsManager(
      device_id, video_effects_manager.InitWithNewPipeAndPassReceiver());

  auto* video_effects_service = GetVideoEffectsService();
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

VideoEffectsManagerImpl& MediaEffectsService::GetOrCreateVideoEffectsManager(
    const std::string& device_id) {
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
  video_effects_managers_.erase(device_id);
}
