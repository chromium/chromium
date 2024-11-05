// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/media_preview/camera_preview/camera_coordinator.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "chrome/browser/media_effects/media_effects_manager_binder.h"
#include "chrome/browser/ui/views/media_preview/camera_preview/camera_mediator.h"
#include "chrome/browser/ui/views/media_preview/media_view.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/video_source.mojom.h"

CameraCoordinator::CameraCoordinator(
    views::View& parent_view,
    bool needs_borders,
    const std::vector<std::string>& eligible_camera_ids,
    bool allow_device_selection,
    base::WeakPtr<content::BrowserContext> browser_context,
    const media_preview_metrics::Context& metrics_context)
    : prefs_(user_prefs::UserPrefs::Get(browser_context.get())),
      combobox_model_({}),
      camera_mediator_(
          *prefs_,
          base::BindRepeating(&CameraCoordinator::OnVideoSourceInfosReceived,
                              base::Unretained(this))),
      eligible_camera_ids_(eligible_camera_ids),
      browser_context_(browser_context),
      allow_device_selection_(allow_device_selection),
      metrics_context_(metrics_context.ui_location,
                       media_preview_metrics::PreviewType::kCamera) {
  auto* camera_view = parent_view.AddChildView(std::make_unique<MediaView>());
  camera_view_tracker_.SetView(camera_view);
  // Safe to use base::Unretained() because `this` owns / outlives
  // `camera_view_tracker_`.
  camera_view_tracker_.SetIsDeletingCallback(base::BindOnce(
      &CameraCoordinator::ResetViewController, base::Unretained(this)));

  // Safe to use base::Unretained() because `this` owns / outlives
  // `camera_view_controller_`.
  camera_view_controller_.emplace(
      *camera_view, needs_borders, combobox_model_, allow_device_selection_,
      base::BindRepeating(&CameraCoordinator::OnVideoSourceChanged,
                          base::Unretained(this)),
      metrics_context_);

  if (base::FeatureList::IsEnabled(media::kCameraMicEffects)) {
    blur_switch_view_controller_.emplace(*camera_view, browser_context_);
  }

  video_stream_coordinator_.emplace(
      camera_view_controller_->GetLiveFeedContainer(), metrics_context_);

  camera_mediator_.InitializeDeviceList();
}

CameraCoordinator::~CameraCoordinator() {
  if (allow_device_selection_ && camera_mediator_.IsDeviceListInitialized()) {
    RecordDeviceSelectionTotalDevices(metrics_context_,
                                      eligible_device_infos_.size());
  }
  // As to guarantee that VideoSourceProvider outlive its VideoSource
  // connection, it is passed in here to protect from destruction.
  video_stream_coordinator_->StopAndCleanup(
      camera_mediator_.TakeVideoSourceProvider());
}

void CameraCoordinator::OnVideoSourceInfosReceived(
    const std::vector<media::VideoCaptureDeviceInfo>& device_infos) {
  if (!camera_view_controller_.has_value()) {
    return;
  }

  eligible_device_infos_.clear();
  for (const auto& device_info : device_infos) {
    if (!eligible_camera_ids_.empty() &&
        !eligible_camera_ids_.contains(device_info.descriptor.device_id)) {
      continue;
    }

    eligible_device_infos_.emplace_back(device_info);
  }

  if (eligible_device_infos_.empty()) {
    active_device_id_.clear();
    video_stream_coordinator_->Stop();
    if (blur_switch_view_controller_) {
      blur_switch_view_controller_->ResetConnections();
    }
  }
  camera_view_controller_->UpdateVideoSourceInfos(eligible_device_infos_);
}

void CameraCoordinator::OnVideoSourceChanged(
    std::optional<size_t> selected_index) {
  if (!selected_index.has_value()) {
    return;
  }

  const auto& device_info = eligible_device_infos_.at(selected_index.value());
  if (active_device_id_ == device_info.descriptor.device_id) {
    return;
  }

  active_device_id_ = device_info.descriptor.device_id;
  mojo::Remote<video_capture::mojom::VideoSource> video_source;
  camera_mediator_.BindVideoSource(active_device_id_,
                                   video_source.BindNewPipeAndPassReceiver());

  if (base::FeatureList::IsEnabled(media::kCameraMicEffects) &&
      browser_context_) {
    if (blur_switch_view_controller_) {
      blur_switch_view_controller_->BindVideoEffectsManager(active_device_id_);
    }

    // TODO: Consider moving this to `CameraMediator` when the code becomes more
    // permanent.
    mojo::PendingRemote<video_effects::mojom::VideoEffectsProcessor>
        video_effects_processor;

    media_effects::BindVideoEffectsProcessor(
        active_device_id_, browser_context_.get(),
        video_effects_processor.InitWithNewPipeAndPassReceiver());
    video_source->RegisterVideoEffectsProcessor(
        std::move(video_effects_processor));
  }

  video_stream_coordinator_->ConnectToDevice(device_info,
                                             std::move(video_source));
}

void CameraCoordinator::OnPermissionChange(bool has_permission) {
  video_stream_coordinator_->OnPermissionChange(has_permission);
}

void CameraCoordinator::UpdateDevicePreferenceRanking() {
  if (active_device_id_.empty()) {
    return;
  }

  auto active_device_iter =
      std::find_if(eligible_device_infos_.begin(), eligible_device_infos_.end(),
                   [&active_device_id = std::as_const(active_device_id_)](
                       const media::VideoCaptureDeviceInfo info) {
                     return info.descriptor.device_id == active_device_id;
                   });
  // The machinery that sets `active_device_id_` and `eligible_device_infos_`
  // ensures that this condition is true.
  CHECK(active_device_iter != eligible_device_infos_.end());

  media_prefs::UpdateVideoDevicePreferenceRanking(*prefs_, active_device_iter,
                                                  eligible_device_infos_);

  video_stream_coordinator_->OnClosing();
}

void CameraCoordinator::ResetViewController() {
  camera_view_controller_.reset();
  blur_switch_view_controller_.reset();
}
