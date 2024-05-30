// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/media_coordinator.h"

#include <memory>

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/media_preview_metrics.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "ui/color/color_id.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

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
    PrefService& prefs,
    bool allow_device_selection,
    const media_preview_metrics::Context& metrics_context) {
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

    media_view_->SetBorder(views::CreateThemedRoundedRectBorder(
        kBorderThickness, kRoundedRadius, ui::kColorSysSurface4));
    media_view_->SetBackground(views::CreateThemedRoundedRectBackground(
        ui::kColorSysSurface4, kRoundedRadius));
  }

  if (view_type != ViewType::kMicOnly) {
    camera_coordinator_.emplace(*media_view_, /*needs_borders=*/!is_subsection,
                                eligible_devices.cameras, prefs,
                                allow_device_selection, metrics_context);
  }

  if (view_type != ViewType::kCameraOnly) {
    mic_coordinator_.emplace(*media_view_, /*needs_borders=*/!is_subsection,
                             eligible_devices.mics, prefs,
                             allow_device_selection, metrics_context);
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

}  // namespace media_coordinator
