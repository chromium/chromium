// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "video_geometry_setter_service.h"

#include <utility>

#include "base/task/sequenced_task_runner.h"

#define MAKE_SURE_ON_SEQUENCE(callback, ...)                                   \
  if (!task_runner_->RunsTasksInCurrentSequence()) {                           \
    task_runner_->PostTask(                                                    \
        FROM_HERE, base::BindOnce(&VideoGeometrySetterService::callback,       \
                                  weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                    \
  }

namespace chromecast {
namespace media {

VideoGeometrySetterService::VideoGeometrySetterService()
    : task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      weak_factory_(this) {}

VideoGeometrySetterService::~VideoGeometrySetterService() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void VideoGeometrySetterService::GetVideoGeometryChangeSubscriber(
    mojo::PendingReceiver<mojom::VideoGeometryChangeSubscriber>
        pending_receiver) {
  MAKE_SURE_ON_SEQUENCE(GetVideoGeometryChangeSubscriber,
                        std::move(pending_receiver));
  video_geometry_change_subscriber_receivers_.Add(this,
                                                  std::move(pending_receiver));
}
void VideoGeometrySetterService::GetVideoGeometrySetter(
    mojo::PendingReceiver<mojom::VideoGeometrySetter> pending_receiver) {
  MAKE_SURE_ON_SEQUENCE(GetVideoGeometrySetter, std::move(pending_receiver));
  if (video_geometry_setter_receiver_.is_bound()) {
    LOG(ERROR) << __func__ << " VideoGeometrySetter dropped";
    video_geometry_setter_receiver_.reset();
  }
  video_geometry_setter_receiver_.Bind(std::move(pending_receiver));
}

void VideoGeometrySetterService::SubscribeToVideoGeometryChange(
    const base::UnguessableToken& overlay_plane_id,
    mojo::PendingRemote<mojom::VideoGeometryChangeClient> client_pending_remote,
    SubscribeToVideoGeometryChangeCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto client = mojo::Remote<mojom::VideoGeometryChangeClient>(
      std::move(client_pending_remote));
  // The remote end closes the message pipe for the client when it no longer
  // wants to receive updates.
  // If the disconnect_handler is called, |this| must be alive, so Unretained is
  // safe.
  client.set_disconnect_handler(base::BindOnce(
      &VideoGeometrySetterService::OnVideoGeometryChangeClientGone,
      base::Unretained(this), overlay_plane_id));
  video_geometry_change_clients_[overlay_plane_id] = std::move(client);

  std::move(callback).Run();
}

void VideoGeometrySetterService::SetVideoGeometry(
    const gfx::RectF& rect_f,
    gfx::OverlayTransform transform,
    const base::UnguessableToken& overlay_plane_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto video_geometry_change_client =
      video_geometry_change_clients_.find(overlay_plane_id);
  if (video_geometry_change_client != video_geometry_change_clients_.end()) {
    video_geometry_change_client->second->OnVideoGeometryChange(rect_f,
                                                                transform);
  }
}

// When a VideoGeometryChangeClient is gone, delete the corresponding entry in
// the mapping.
void VideoGeometrySetterService::OnVideoGeometryChangeClientGone(
    const base::UnguessableToken overlay_plane_id) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  video_geometry_change_clients_.erase(overlay_plane_id);
}

}  // namespace media
}  // namespace chromecast
