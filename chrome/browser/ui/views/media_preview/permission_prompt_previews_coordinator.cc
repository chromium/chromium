// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/permission_prompt_previews_coordinator.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "chrome/browser/ui/views/media_preview/scroll_media_preview.h"

PermissionPromptPreviewsCoordinator::PermissionPromptPreviewsCoordinator(
    Profile* profile,
    views::View* parent_view,
    size_t index,
    MediaCoordinator::ViewType view_type,
    std::vector<std::string> requested_audio_capture_device_ids,
    std::vector<std::string> requested_video_capture_device_ids)
    : view_type_(view_type) {
  CHECK(parent_view);
  CHECK(profile);

  auto* container_view =
      scroll_media_preview::CreateScrollViewAndGetContents(*parent_view, index);

  const auto eligible_devices = MediaCoordinator::EligibleDevices{
      /*cameras=*/std::move(requested_video_capture_device_ids),
      /*mics=*/std::move(requested_audio_capture_device_ids)};

  const auto metrics_context = media_preview_metrics::Context(
      media_preview_metrics::UiLocation::kPermissionPrompt,
      media_coordinator::GetPreviewTypeFromMediaCoordinatorViewType(
          view_type_));

  media_preview_coordinator_.emplace(
      view_type_, *container_view,
      /*is_subsection=*/false, eligible_devices, profile->GetWeakPtr(),
      /*allow_device_selection=*/true, metrics_context);

  start_time_ = base::TimeTicks::Now();
}

PermissionPromptPreviewsCoordinator::~PermissionPromptPreviewsCoordinator() {
  media_preview_metrics::RecordMediaPreviewDuration(
      {media_preview_metrics::UiLocation::kPermissionPrompt,
       media_coordinator::GetPreviewTypeFromMediaCoordinatorViewType(
           view_type_)},
      base::TimeTicks::Now() - start_time_);
}

void PermissionPromptPreviewsCoordinator::UpdateDevicePreferenceRanking() {
  if (media_preview_coordinator_) {
    media_preview_coordinator_->UpdateDevicePreferenceRanking();
  }
}
