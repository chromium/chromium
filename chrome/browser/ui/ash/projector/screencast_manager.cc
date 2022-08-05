// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/screencast_manager.h"

#include <memory>

#include "ash/webui/projector_app/projector_screencast.h"

namespace ash {

ScreencastManager::ScreencastManager() = default;
ScreencastManager::~ScreencastManager() = default;

void ScreencastManager::GetVideo(
    const std::string& video_file_id,
    const std::string& resource_key,
    ProjectorAppClient::OnGetVideoCallback callback) const {
  auto video = std::make_unique<ProjectorScreencastVideo>();
  video->file_id = video_file_id;
  // TODO(b/237089852):
  // 1. Find the video file by id.
  // 2. Find the video duration.
  // 3. Launch the app with the video file.
  std::move(callback).Run(std::move(video), /*error_message=*/std::string());
}

}  // namespace ash
