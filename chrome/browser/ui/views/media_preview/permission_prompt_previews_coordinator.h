// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_PERMISSION_PROMPT_PREVIEWS_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_PERMISSION_PROMPT_PREVIEWS_COORDINATOR_H_

#include "chrome/browser/ui/views/media_preview/media_coordinator.h"

class Browser;

class PermissionPromptPreviewsCoordinator {
 public:
  PermissionPromptPreviewsCoordinator(
      Browser* browser,
      views::View* parent_view,
      size_t index,
      std::vector<std::string> requested_audio_capture_device_ids,
      std::vector<std::string> requested_video_capture_device_ids);
  PermissionPromptPreviewsCoordinator(
      const PermissionPromptPreviewsCoordinator&) = delete;
  PermissionPromptPreviewsCoordinator& operator=(
      const PermissionPromptPreviewsCoordinator&) = delete;
  ~PermissionPromptPreviewsCoordinator();

  void UpdateDevicePreferenceRanking();

  MediaCoordinator::ViewType GetViewTypeForTesting() const {
    return view_type_;
  }

 private:
  const MediaCoordinator::ViewType view_type_;
  std::optional<MediaCoordinator> media_preview_coordinator_;
  base::TimeTicks start_time_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_PREVIEW_PERMISSION_PROMPT_PREVIEWS_COORDINATOR_H_
