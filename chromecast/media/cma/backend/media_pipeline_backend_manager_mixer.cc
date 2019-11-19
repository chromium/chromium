// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/media_pipeline_backend_manager.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chromecast/media/audio/mixer_service/control_connection.h"

namespace chromecast {
namespace media {

void MediaPipelineBackendManager::CreateMixerConnection() {
  struct RealMixerConnection : public MixerConnection {
    RealMixerConnection() { connection.Connect(); }

    ~RealMixerConnection() override = default;

    mixer_service::ControlConnection connection;
  };

  DCHECK(media_task_runner_->BelongsToCurrentThread());
  auto mixer = std::make_unique<RealMixerConnection>();
  mixer->connection.SetStreamCountCallback(base::BindRepeating(
      &MediaPipelineBackendManager::OnMixerStreamCountChange,
      base::Unretained(this)));
  mixer_connection_ = std::move(mixer);
}

}  // namespace media
}  // namespace chromecast
