// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_device_list_host.h"

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/global_media_controls/media_item_ui_metrics.h"
#include "chrome/browser/ui/media_router/cast_dialog_model.h"
#include "chrome/browser/ui/media_router/ui_media_sink.h"
#include "components/global_media_controls/public/constants.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/media_router/browser/media_router_metrics.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/mojom/media_route_provider_id.mojom.h"

using global_media_controls::mojom::IconType;

namespace {

IconType GetIcon(const media_router::UIMediaSink& sink) {
  if (sink.state == media_router::UIMediaSinkState::CONNECTING ||
      sink.state == media_router::UIMediaSinkState::DISCONNECTING) {
    return IconType::kThrobber;
  }
  if (sink.issue) {
    return IconType::kInfo;
  }
  switch (sink.icon_type) {
    case media_router::SinkIconType::CAST:
    case media_router::SinkIconType::GENERIC:
      return IconType::kTv;
    case media_router::SinkIconType::CAST_AUDIO:
      return IconType::kSpeaker;
    case media_router::SinkIconType::CAST_AUDIO_GROUP:
      return IconType::kSpeakerGroup;
    case media_router::SinkIconType::WIRED_DISPLAY:
      return IconType::kInput;
    case media_router::SinkIconType::TOTAL_COUNT:
      NOTREACHED_IN_MIGRATION();
      return IconType::kTv;
  }
}

bool SupportsTabAudioMirroring(media_router::CastModeSet cast_mode,
                               media_router::SinkIconType icon_type) {
  return base::FeatureList::IsEnabled(
             media_router::kFallbackToAudioTabMirroring) &&
         base::Contains(cast_mode, media_router::MediaCastMode::TAB_MIRROR) &&
         (icon_type == media_router::SinkIconType::CAST_AUDIO ||
          icon_type == media_router::SinkIconType::CAST_AUDIO_GROUP);
}

std::optional<media_router::MediaCastMode> GetPreferredCastMode(
    media_router::CastModeSet cast_mode,
    media_router::SinkIconType icon_type) {
  if (base::Contains(cast_mode, media_router::MediaCastMode::PRESENTATION)) {
    return media_router::MediaCastMode::PRESENTATION;
  } else if (base::Contains(cast_mode,
                            media_router::MediaCastMode::REMOTE_PLAYBACK)) {
    return media_router::MediaCastMode::REMOTE_PLAYBACK;
  } else if (SupportsTabAudioMirroring(cast_mode, icon_type)) {
    return media_router::MediaCastMode::TAB_MIRROR;
  }
  return std::nullopt;
}

global_media_controls::mojom::DevicePtr CreateDevice(
    const media_router::UIMediaSink& sink) {
  global_media_controls::mojom::DevicePtr device =
      global_media_controls::mojom::Device::New();
  device->id = sink.id;
  device->name = base::UTF16ToUTF8(sink.friendly_name);
  device->status_text = base::UTF16ToUTF8(sink.GetStatusTextForDisplay());
  device->icon = GetIcon(sink);
  return device;
}

}  // namespace

CastDeviceListHost::CastDeviceListHost(
    std::unique_ptr<media_router::CastDialogController> dialog_controller,
    mojo::PendingRemote<global_media_controls::mojom::DeviceListClient> client,
    base::RepeatingClosure media_remoting_callback,
    base::RepeatingClosure hide_dialog_callback,
    base::RepeatingClosure on_sinks_discovered_callback)
    : cast_controller_(std::move(dialog_controller)),
      client_(std::move(client)),
      media_remoting_callback_(std::move(media_remoting_callback)),
      hide_dialog_callback_(std::move(hide_dialog_callback)),
      on_sinks_discovered_callback_(std::move(on_sinks_discovered_callback)),
      initialization_time_(base::Time::Now()),
      id_(next_id_++) {
  cast_controller_->AddObserver(this);
  cast_controller_->RegisterDestructor(
      base::BindOnce(&CastDeviceListHost::DestroyCastController,
                     // Unretained is safe: this callback is held by
                     // `cast_controller_`, which is owned by this object.
                     base::Unretained(this)));
}

CastDeviceListHost::~CastDeviceListHost() {
  if (cast_controller_) {
    cast_controller_->RemoveObserver(this);
  }
}

void CastDeviceListHost::SelectDevice(const std::string& device_id) {
  if (!cast_controller_) {
    return;
  }
  auto sink_it =
      std::find_if(sinks_.begin(), sinks_.end(),
                   [&device_id](const media_router::UIMediaSink& sink) {
                     return sink.id == device_id;
                   });
  if (sink_it == sinks_.end()) {
    return;
  }
  const media_router::UIMediaSink& sink = *sink_it;
  // Clicking on the device entry with an issue will clear the issue without
  // starting casting.
  if (sink.issue) {
    cast_controller_->ClearIssue(sink.issue->id());
    return;
  }
  // When users click on a CONNECTED sink,
  // if it is a CAST sink, a new cast session will replace the existing cast
  // session.
  // if it is a DIAL sink, the existing session will be terminated and users
  // need to click on the sink again to start a new session.
  // TODO(crbug.com/1206830): implement "terminate existing route and start a
  // new session" in DIAL MRP.
  if (sink.state == media_router::UIMediaSinkState::AVAILABLE) {
    StartCasting(sink);
  } else if (sink.state == media_router::UIMediaSinkState::CONNECTED) {
    // We record stopping casting here even if we are starting casting, because
    // the existing session is being stopped and replaced by a new session.
    if (sink.provider == media_router::mojom::MediaRouteProviderId::DIAL) {
      DCHECK(sink.route);
      MediaItemUIMetrics::RecordStopCastingMetrics(
          media_router::MediaCastMode::PRESENTATION);
      cast_controller_->StopCasting(sink.route->media_route_id());
    } else {
      StartCasting(sink);
    }
  }
}

void CastDeviceListHost::OnModelUpdated(
    const media_router::CastDialogModel& model) {
  if (base::FeatureList::IsEnabled(
          media_router::kShowCastPermissionRejectedError) &&
      model.is_permission_rejected()) {
    client_->OnPermissionRejected();
    return;
  }

  sinks_ = model.media_sinks();
  std::vector<global_media_controls::mojom::DevicePtr> devices;
  for (const auto& sink : sinks_) {
    if (GetPreferredCastMode(sink.cast_modes, sink.icon_type)) {
      devices.push_back(CreateDevice(sink));
    }
  }

  if (!devices.empty()) {
    on_sinks_discovered_callback_.Run();
    RecordSinkLoadTime();
  }
  client_->OnDevicesUpdated(std::move(devices));
}

void CastDeviceListHost::OnCastingStarted() {
  hide_dialog_callback_.Run();
}

void CastDeviceListHost::StartCasting(const media_router::UIMediaSink& sink) {
  auto cast_mode = GetPreferredCastMode(sink.cast_modes, sink.icon_type);
  if (!cast_mode) {
    // The UI calls this method asynchronously over Mojo, so by the time this
    // gets called it's possible for the set of Cast modes to no longer be
    // valid.
    return;
  }
  cast_controller_->StartCasting(sink.id, cast_mode.value());
  if (cast_mode.value() == media_router::MediaCastMode::REMOTE_PLAYBACK) {
    media_remoting_callback_.Run();
  }
  MediaItemUIMetrics::RecordStartCastingMetrics(sink.icon_type,
                                                cast_mode.value());
}

void CastDeviceListHost::DestroyCastController() {
  cast_controller_.reset();
}

void CastDeviceListHost::RecordSinkLoadTime() {
  if (!sinks_load_time_.is_null()) {
    return;
  }
  sinks_load_time_ = base::Time::Now();
  media_router::MediaRouterMetrics::RecordGmcDialogLoaded(sinks_load_time_ -
                                                          initialization_time_);
}

int CastDeviceListHost::next_id_ = 0;
