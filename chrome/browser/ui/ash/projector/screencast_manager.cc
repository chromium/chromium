// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/screencast_manager.h"

#include <memory>

#include "ash/webui/projector_app/projector_screencast.h"
#include "base/check.h"

namespace ash {

ScreencastManager::ScreencastManager() = default;
ScreencastManager::~ScreencastManager() = default;

// TODO(b/237089852): Find the path for local video file.
// TODO(b/236857019): Set video and metadata file ids and rest fields for
// screencasts.
void ScreencastManager::GetScreencast(
    const std::string& screencast_id,
    ProjectorAppClient::OnGetScreencastCallback callback) {
  DCHECK(!screencast_id.empty());
  std::unique_ptr<ProjectorScreencast> screencast =
      std::make_unique<ProjectorScreencast>();
  screencast->container_folder_id = screencast_id;
  std::move(callback).Run(std::move(screencast), std::string());
}

}  // namespace ash
