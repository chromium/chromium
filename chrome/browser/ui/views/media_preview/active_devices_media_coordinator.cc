// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/active_devices_media_coordinator.h"

#include <string>
#include <vector>

#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "ui/views/view.h"

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

}  // namespace

ActiveDevicesMediaCoordinator::ActiveDevicesMediaCoordinator(
    content::WebContents* web_contents,
    MediaCoordinator::ViewType view_type,
    views::View* parent_view)
    : view_type_(view_type),
      parent_view_(parent_view),
      stream_type_(view_type_ == MediaCoordinator::ViewType::kCameraOnly
                       ? blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE
                       : blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) {
  CHECK(web_contents);
  web_contents_ = web_contents->GetWeakPtr();

  CHECK(parent_view_);
  container_ = parent_view_->AddChildView(std::make_unique<MediaView>());
  CHECK(container_);

  MediaCaptureDevicesDispatcher::GetInstance()->AddObserver(this);
  UpdateMediaCoordinatorList();
}

ActiveDevicesMediaCoordinator::~ActiveDevicesMediaCoordinator() {
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
          base::Unretained(this)));
}

void ActiveDevicesMediaCoordinator::GotDeviceIdsOpenedForWebContents(
    std::vector<std::string> active_device_ids) {
  if (active_device_ids.empty()) {
    media_coordinators_.clear();
    AddMediaCoordinatorForDevice(/*active_device_id=*/
                                 std::nullopt);
    return;
  }

  base::flat_set<std::string> active_device_id_set{active_device_ids};
  for (const auto& key : GetMediaCoordinatorKeys()) {
    if (!active_device_id_set.erase(key)) {
      // remove the coordinator because it isn't active anymore.
      media_coordinators_.erase(key);
    }
  }

  for (const auto& active_device_id : active_device_id_set) {
    AddMediaCoordinatorForDevice(active_device_id);
  }
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
      coordinator_key, std::make_unique<MediaCoordinator>(
                           view_type_, *container_,
                           /*index=*/std::nullopt,
                           /*is_subsection=*/true, eligible_devices, *prefs));
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
