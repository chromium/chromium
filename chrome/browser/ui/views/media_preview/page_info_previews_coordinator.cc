// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/page_info_previews_coordinator.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "chrome/browser/ui/views/media_preview/scroll_media_preview.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

MediaCoordinator::ViewType ComputePreviewType(
    ContentSettingsType content_settings_type) {
  if (content_settings_type == ContentSettingsType::MEDIASTREAM_CAMERA ||
      content_settings_type == ContentSettingsType::CAMERA_PAN_TILT_ZOOM) {
    return MediaCoordinator::ViewType::kCameraOnly;
  }

  if (content_settings_type == ContentSettingsType::MEDIASTREAM_MIC) {
    return MediaCoordinator::ViewType::kMicOnly;
  }

  // We always expect that `content_settings_type` is either MEDIASTREAM_CAMERA,
  // CAMERA_PAN_TILT_ZOOM or MEDIASTREAM_MIC.
  NOTREACHED();
}

}  // namespace

PageInfoPreviewsCoordinator::PageInfoPreviewsCoordinator(
    content::WebContents* web_contents,
    ContentSettingsType content_settings_type,
    views::View* parent_view)
    : view_type_(ComputePreviewType(content_settings_type)),
      metrics_context_(media_preview_metrics::Context(
          media_preview_metrics::UiLocation::kPageInfo,
          media_coordinator::GetPreviewTypeFromMediaCoordinatorViewType(
              view_type_))) {
  CHECK(web_contents);

  CHECK(parent_view);
  auto* scroll_contents =
      scroll_media_preview::CreateScrollViewAndGetContents(*parent_view);
  CHECK(scroll_contents);

  auto* container =
      scroll_contents->AddChildView(std::make_unique<MediaView>());
  const auto distance_related_control =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL);
  container->SetBetweenChildSpacing(distance_related_control);
  container->SetProperty(views::kMarginsKey,
                         gfx::Insets::VH(distance_related_control, 0));

  active_devices_coordinator_.emplace(web_contents->GetWeakPtr(), view_type_,
                                      container, metrics_context_);

  media_preview_start_time_ = base::TimeTicks::Now();
}

PageInfoPreviewsCoordinator::~PageInfoPreviewsCoordinator() {
  media_preview_metrics::RecordMediaPreviewDuration(
      metrics_context_, base::TimeTicks::Now() - media_preview_start_time_);
}

void PageInfoPreviewsCoordinator::UpdateDevicePreferenceRanking() {
  active_devices_coordinator_->UpdateDevicePreferenceRanking();
}

void PageInfoPreviewsCoordinator::OnPermissionChange(bool has_permission) {
  active_devices_coordinator_->OnPermissionChange(has_permission);
}
