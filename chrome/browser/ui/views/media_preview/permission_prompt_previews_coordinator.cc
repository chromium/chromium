// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/permission_prompt_previews_coordinator.h"

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "chrome/browser/ui/views/media_preview/scroll_media_preview.h"

namespace {

MediaCoordinator::ViewType ComputePreviewType(
    std::vector<std::string> requested_audio_capture_device_ids,
    std::vector<std::string> requested_video_capture_device_ids) {
  if (!requested_audio_capture_device_ids.empty() &&
      !requested_video_capture_device_ids.empty()) {
    return MediaCoordinator::ViewType::kBoth;
  }
  if (!requested_video_capture_device_ids.empty()) {
    return MediaCoordinator::ViewType::kCameraOnly;
  }
  if (!requested_audio_capture_device_ids.empty()) {
    return MediaCoordinator::ViewType::kMicOnly;
  }
  // We always expect that at least one of the 2 vectors is not empty.
  NOTREACHED();
}

}  // namespace

PermissionPromptPreviewsCoordinator::PermissionPromptPreviewsCoordinator(
    Browser* browser,
    views::View* parent_view,
    size_t index,
    std::vector<std::string> requested_audio_capture_device_ids,
    std::vector<std::string> requested_video_capture_device_ids)
    : view_type_(ComputePreviewType(requested_audio_capture_device_ids,
                                    requested_video_capture_device_ids)) {
  CHECK(parent_view);
  CHECK(browser);
  CHECK(browser->profile());

  auto* container_view =
      scroll_media_preview::CreateScrollViewAndGetContents(*parent_view, index);

  const auto eligible_devices = MediaCoordinator::EligibleDevices{
      /*cameras=*/requested_video_capture_device_ids,
      /*mics=*/requested_audio_capture_device_ids};

  const auto metrics_context = media_preview_metrics::Context(
      media_preview_metrics::UiLocation::kPermissionPrompt,
      media_coordinator::GetPreviewTypeFromMediaCoordinatorViewType(
          view_type_));

  media_preview_coordinator_.emplace(view_type_, *container_view,
                                     /*is_subsection=*/false, eligible_devices,
                                     *browser->profile()->GetPrefs(),
                                     /*allow_device_selection=*/true,
                                     metrics_context);

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
