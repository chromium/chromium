// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_coordinator.h"

#include <algorithm>
#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request.h"
#include "components/permissions/request_type.h"
#include "components/user_prefs/user_prefs.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace {
base::WeakPtr<permissions::PermissionRequest> FindPermissionRequest(
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
    permissions::RequestType request_type) {
  if (!delegate) {
    return nullptr;
  }

  auto request_it =
      std::ranges::find(delegate->Requests(), request_type,
                        &permissions::PermissionRequest::request_type);

  if (request_it != delegate->Requests().end()) {
    return (*request_it)->GetWeakPtr();
  }

  return nullptr;
}
}  // namespace

MediaCoordinator::EligibleDevices::EligibleDevices() = default;
MediaCoordinator::EligibleDevices::EligibleDevices(
    std::vector<std::string> cameras,
    std::vector<std::string> mics)
    : cameras(cameras), mics(mics) {}
MediaCoordinator::EligibleDevices::~EligibleDevices() = default;
MediaCoordinator::EligibleDevices::EligibleDevices(const EligibleDevices&) =
    default;

MediaCoordinator::MediaCoordinator(
    ViewType view_type,
    views::View& parent_view,
    bool is_subsection,
    EligibleDevices eligible_devices,
    base::WeakPtr<content::BrowserContext> browser_context,
    bool allow_device_selection,
    const media_preview_metrics::Context& metrics_context,
    base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate) {
  media_view_ =
      parent_view.AddChildView(std::make_unique<MediaView>(is_subsection));
  media_view_->SetBetweenChildSpacing(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL));

  if (!is_subsection) {
    auto* provider = ChromeLayoutProvider::Get();
    const int kRoundedRadius = provider->GetCornerRadiusMetric(
        views::ShapeContextTokens::kOmniboxExpandedRadius);
    const int kBorderThickness =
        provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_VERTICAL);

    media_view_->SetBorder(views::CreateRoundedRectBorder(
        kBorderThickness, kRoundedRadius, ui::kColorSysSurface4));
    media_view_->SetBackground(views::CreateRoundedRectBackground(
        ui::kColorSysSurface4, kRoundedRadius));
  }

  if (view_type != ViewType::kMicOnly) {
    auto camera_request = FindPermissionRequest(
        delegate, permissions::RequestType::kCameraStream);
    camera_coordinator_.emplace(
        *media_view_, /*needs_borders=*/!is_subsection,
        eligible_devices.cameras, allow_device_selection, browser_context,
        media_preview_metrics::Context(
            metrics_context.ui_location, metrics_context.preview_type,
            metrics_context.prompt_type, camera_request));
  }

  if (view_type != ViewType::kCameraOnly) {
    auto mic_request =
        FindPermissionRequest(delegate, permissions::RequestType::kMicStream);
    mic_coordinator_.emplace(
        *media_view_, /*needs_borders=*/!is_subsection, eligible_devices.mics,
        *user_prefs::UserPrefs::Get(browser_context.get()),
        allow_device_selection,
        media_preview_metrics::Context(
            metrics_context.ui_location, metrics_context.preview_type,
            metrics_context.prompt_type, mic_request));
  }
}

MediaCoordinator::~MediaCoordinator() {
  // Reset child coordinators before removing view.
  camera_coordinator_.reset();
  mic_coordinator_.reset();
  if (media_view_ && media_view_->parent()) {
    media_view_->parent()->RemoveChildViewT(
        std::exchange(media_view_, nullptr));
  }
}

void MediaCoordinator::UpdateDevicePreferenceRanking() {
  if (camera_coordinator_) {
    camera_coordinator_->UpdateDevicePreferenceRanking();
  }
  if (mic_coordinator_) {
    mic_coordinator_->UpdateDevicePreferenceRanking();
  }
}

void MediaCoordinator::OnCameraPermissionChange(bool has_permission) {
  if (camera_coordinator_) {
    camera_coordinator_->OnPermissionChange(has_permission);
  }
}

namespace media_coordinator {

media_preview_metrics::PreviewType GetPreviewTypeFromMediaCoordinatorViewType(
    MediaCoordinator::ViewType view_type) {
  switch (view_type) {
    case MediaCoordinator::ViewType::kBoth:
      return media_preview_metrics::PreviewType::kCameraAndMic;
    case MediaCoordinator::ViewType::kCameraOnly:
      return media_preview_metrics::PreviewType::kCamera;
    case MediaCoordinator::ViewType::kMicOnly:
      return media_preview_metrics::PreviewType::kMic;
  }
}

media_preview_metrics::PromptType GetPromptTypeFromMediaCoordinatorViewType(
    MediaCoordinator::ViewType view_type) {
  switch (view_type) {
    case MediaCoordinator::ViewType::kBoth:
      return media_preview_metrics::PromptType::kCombined;
    case MediaCoordinator::ViewType::kCameraOnly:
      [[fallthrough]];
    case MediaCoordinator::ViewType::kMicOnly:
      return media_preview_metrics::PromptType::kSingle;
  }
}

}  // namespace media_coordinator
