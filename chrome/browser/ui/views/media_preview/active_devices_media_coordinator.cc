// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/active_devices_media_coordinator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "chrome/browser/ui/views/media_preview/scroll_media_preview.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr char kMutableCoordinatorId[] = "changeable";

using EligibleDevices = MediaCoordinator::EligibleDevices;

bool IsWithinWebContents(content::GlobalRenderFrameHostId render_frame_host_id,
                         base::WeakPtr<content::WebContents> web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool is_request_in_frame = false;
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&is_request_in_frame,
       render_frame_host_id](content::RenderFrameHost* render_frame_host) {
        if (render_frame_host->GetGlobalId() == render_frame_host_id) {
          is_request_in_frame = true;
        }
      });
  return is_request_in_frame;
}

std::unique_ptr<views::Separator> CreateSeparator() {
  auto separator = std::make_unique<views::Separator>();

  const int horizontal_inset = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_HORIZONTAL_SEPARATOR_PADDING_PAGE_INFO_VIEW);

  separator->SetProperty(views::kMarginsKey,
                         gfx::Insets::VH(0, horizontal_inset));
  return separator;
}

}  // namespace

ActiveDevicesMediaCoordinator::ActiveDevicesMediaCoordinator(
    content::WebContents* web_contents,
    MediaCoordinator::ViewType view_type,
    views::View* parent_view)
    : view_type_(view_type),
      stream_type_(view_type_ == MediaCoordinator::ViewType::kCameraOnly
                       ? blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE
                       : blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE),
      media_preview_metrics_context_(
          media_preview_metrics::UiLocation::kPageInfo,
          media_coordinator::GetPreviewTypeFromMediaCoordinatorViewType(
              view_type_)) {
  CHECK(web_contents);
  web_contents_ = web_contents->GetWeakPtr();

  CHECK(parent_view);
  auto* scroll_contents =
      scroll_media_preview::CreateScrollViewAndGetContents(*parent_view);
  CHECK(scroll_contents);

  container_ = scroll_contents->AddChildView(std::make_unique<MediaView>());
  auto distance_related_control =
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          views::DISTANCE_RELATED_CONTROL_VERTICAL);
  container_->SetBetweenChildSpacing(distance_related_control);
  container_->SetProperty(views::kMarginsKey,
                          gfx::Insets::VH(distance_related_control, 0));

  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
  UpdateMediaCoordinatorList();

  media_preview_start_time_ = base::TimeTicks::Now();
}

ActiveDevicesMediaCoordinator::~ActiveDevicesMediaCoordinator() {
  media_preview_metrics::RecordMediaPreviewDuration(
      media_preview_metrics_context_,
      base::TimeTicks::Now() - media_preview_start_time_);

  MediaCaptureDevicesDispatcher::GetInstance()->RemoveObserver(this);
}

void ActiveDevicesMediaCoordinator::UpdateDevicePreferenceRanking() {
  // A mutable coordinator will only be present in the case that there is a
  // single coordinator, so return early if that isn't the case.
  if (media_coordinators_.size() != 1) {
    return;
  }

  if (auto mutable_coordinator =
          media_coordinators_.find(kMutableCoordinatorId);
      mutable_coordinator != media_coordinators_.end()) {
    mutable_coordinator->second->UpdateDevicePreferenceRanking();
  }
}

void ActiveDevicesMediaCoordinator::UpdateMediaCoordinatorList() {
  if (!web_contents_.MaybeValid()) {
    return;
  }

  web_contents_->GetMediaCaptureRawDeviceIdsOpened(
      stream_type_,
      base::BindOnce(
          &ActiveDevicesMediaCoordinator::GotDeviceIdsOpenedForWebContents,
          weak_ptr_factory_.GetWeakPtr()));
}

void ActiveDevicesMediaCoordinator::GotDeviceIdsOpenedForWebContents(
    std::vector<std::string> active_device_ids) {
  if (view_type_ == MediaCoordinator::ViewType::kCameraOnly) {
    media_preview_metrics::RecordPageInfoCameraNumInUseDevices(
        active_device_ids.size());
  } else {
    media_preview_metrics::RecordPageInfoMicNumInUseDevices(
        active_device_ids.size());
  }

  if (active_device_ids.empty()) {
    if (media_coordinators_.contains(kMutableCoordinatorId)) {
      return;
    }
    media_coordinators_.clear();
    separators_.clear();
    // RemoveAllChildViews() is called to delete all separators.
    container_->RemoveAllChildViews();
    AddMediaCoordinatorForDevice(/*active_device_id=*/
                                 std::nullopt);
    CHECK(!container_->children().empty());
    container_->children().back()->SetVisible(false);
    return;
  }

  base::flat_set<std::string> active_device_id_set{active_device_ids};
  for (const auto& key : GetMediaCoordinatorKeys()) {
    const auto& separator = separators_.find(key);
    CHECK(separator != separators_.end());

    if (!active_device_id_set.erase(key)) {
      // remove the coordinator because it isn't active anymore.
      media_coordinators_.erase(key);
      container_->RemoveChildViewT(std::exchange(separator->second, nullptr));
      separators_.erase(separator);
    } else {
      separator->second->SetVisible(true);
    }
  }

  for (const auto& active_device_id : active_device_id_set) {
    AddMediaCoordinatorForDevice(active_device_id);
  }
  CHECK(!container_->children().empty());
  container_->children().back()->SetVisible(false);
}

void ActiveDevicesMediaCoordinator::AddMediaCoordinatorForDevice(
    const std::optional<std::string>& active_device_id) {
  if (!web_contents_.MaybeValid()) {
    return;
  }

  std::vector<std::string> active_device_id_vector;
  if (active_device_id.has_value()) {
    active_device_id_vector.push_back(active_device_id.value());
  }
  EligibleDevices eligible_devices;
  if (view_type_ == MediaCoordinator::ViewType::kCameraOnly) {
    eligible_devices.cameras = active_device_id_vector;
  } else {
    eligible_devices.mics = active_device_id_vector;
  }

  auto coordinator_key = active_device_id.value_or(kMutableCoordinatorId);
  auto* prefs = user_prefs::UserPrefs::Get(web_contents_->GetBrowserContext());
  media_coordinators_.emplace(
      coordinator_key,
      std::make_unique<MediaCoordinator>(
          view_type_, *container_,
          /*is_subsection=*/true, eligible_devices, *prefs,
          /*allow_device_selection=*/!active_device_id.has_value(),
          media_preview_metrics_context_));
  separators_.emplace(coordinator_key,
                      container_->AddChildView(CreateSeparator()));
}

void ActiveDevicesMediaCoordinator::OnRequestUpdate(
    int render_process_id,
    int render_frame_id,
    blink::mojom::MediaStreamType stream_type,
    const content::MediaRequestState state) {
  if (!web_contents_.MaybeValid() || stream_type != stream_type_) {
    return;
  }

  if (!IsWithinWebContents(
          content::GlobalRenderFrameHostId{render_process_id, render_frame_id},
          web_contents_)) {
    return;
  }

  if (state == content::MediaRequestState::MEDIA_REQUEST_STATE_DONE ||
      state == content::MediaRequestState::MEDIA_REQUEST_STATE_CLOSING) {
    UpdateMediaCoordinatorList();
  }
}

std::vector<std::string>
ActiveDevicesMediaCoordinator::GetMediaCoordinatorKeys() {
  std::vector<std::string> keys;
  keys.reserve(media_coordinators_.size());
  for (const auto& [key, _] : media_coordinators_) {
    keys.push_back(key);
  }
  return keys;
}
