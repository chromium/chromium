// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/media_controller.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client.h"
#include "chromeos/ash/services/libassistant/grpc/external_services/grpc_services_observer.h"
#include "chromeos/ash/services/libassistant/grpc/utils/media_status_utils.h"
#include "chromeos/assistant/internal/libassistant/shared_headers.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/delegate/event_handler_interface.pb.h"
#include "chromeos/assistant/internal/proto/shared/proto/v2/device_state_event.pb.h"
#include "chromeos/assistant/internal/util_headers.h"
#include "chromeos/services/assistant/public/shared/utils.h"

namespace ash::libassistant {

namespace {

constexpr char kNextTrackClientOp[] = "media.NEXT";
constexpr char kPauseTrackClientOp[] = "media.PAUSE";
constexpr char kPlayMediaClientOp[] = "media.PLAY_MEDIA";
constexpr char kPrevTrackClientOp[] = "media.PREVIOUS";
constexpr char kResumeTrackClientOp[] = "media.RESUME";
constexpr char kStopTrackClientOp[] = "media.STOP";

constexpr char kIntentActionView[] = "android.intent.action.VIEW";

constexpr char kWebUrlPrefix[] = "http";

using assistant::AndroidAppInfo;
using chromeos::assistant::shared::PlayMediaArgs;

// A macro which ensures we are running on the mojom thread.
#define ENSURE_MOJOM_THREAD(method, ...)                                    \
  if (!mojom_task_runner_->RunsTasksInCurrentSequence()) {                  \
    mojom_task_runner_->PostTask(                                           \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }

std::string GetAndroidIntentUrlFromMediaArgs(
    const std::string& play_media_args_proto) {
  PlayMediaArgs play_media_args;
  if (play_media_args.ParseFromString(play_media_args_proto)) {
    for (auto media_item : play_media_args.media_item()) {
      if (!media_item.has_uri())
        continue;

      return media_item.uri();
    }
  }
  return std::string();
}

std::optional<AndroidAppInfo> GetAppInfoFromMediaArgs(
    const std::string& play_media_args_proto) {
  PlayMediaArgs play_media_args;
  if (play_media_args.ParseFromString(play_media_args_proto)) {
    for (auto& media_item : play_media_args.media_item()) {
      if (media_item.has_provider() &&
          media_item.provider().has_android_app_info()) {
        auto& app_info = media_item.provider().android_app_info();
        AndroidAppInfo result;
        result.package_name = app_info.package_name();
        result.version = app_info.app_version();
        result.localized_app_name = app_info.localized_app_name();
        result.intent = app_info.android_intent();
        return result;
      }
    }
  }
  return std::nullopt;
}

std::string GetWebUrlFromMediaArgs(const std::string& play_media_args_proto) {
  PlayMediaArgs play_media_args;
  if (play_media_args.ParseFromString(play_media_args_proto)) {
    for (auto media_item : play_media_args.media_item()) {
      if (!media_item.has_uri())
        continue;

      // For web url in browser.
      if (base::StartsWith(media_item.uri(), kWebUrlPrefix))
        return media_item.uri();
    }
  }

  return std::string();
}

}  // namespace

class MediaController::GrpcEventsObserver
    : public GrpcServicesObserver<::assistant::api::OnDeviceStateEventRequest>,
      public GrpcServicesObserver<
          ::assistant::api::OnMediaActionFallbackEventRequest> {
 public:
  explicit GrpcEventsObserver(MediaController* parent) : parent_(parent) {}
  GrpcEventsObserver(const GrpcEventsObserver&) = delete;
  GrpcEventsObserver& operator=(const GrpcEventsObserver&) = delete;
  ~GrpcEventsObserver() override = default;

  // GrpcServicesObserver:
  // Invoked when a device state event has been received.
  void OnGrpcMessage(
      const ::assistant::api::OnDeviceStateEventRequest& request) override {
    VLOG(1) << "Sending playback state update";

    if (!request.event().has_on_state_changed())
      return;

    const auto& new_state = request.event().on_state_changed().new_state();
    if (!new_state.has_media_status())
      return;

    delegate().OnPlaybackStateChanged(
        ConvertMediaStatusToMojomFromV2(new_state.media_status()));
  }

  // Invoked when a media action fallack event has been received.
  void OnGrpcMessage(const ::assistant::api::OnMediaActionFallbackEventRequest&
                         request) override {
    if (!request.event().has_on_media_action_event())
      return;

    auto media_action_event = request.event().on_media_action_event();
    HandleMediaAction(media_action_event.action_name(),
                      media_action_event.action_args());
  }

 private:
  void HandleMediaAction(const std::string& action_name,
                         const std::string& media_action_args_proto) {
    if (action_name == kPlayMediaClientOp)
      OnPlayMedia(media_action_args_proto);
    else
      OnMediaControlAction(action_name, media_action_args_proto);
  }

  void OnPlayMedia(const std::string& play_media_args_proto) {
    std::optional<AndroidAppInfo> app_info =
        GetAppInfoFromMediaArgs(play_media_args_proto);
    if (app_info) {
      OnOpenMediaAndroidIntent(play_media_args_proto,
                               std::move(app_info.value()));
    } else {
      OnOpenUrl(play_media_args_proto);
    }
  }

  // Handle android media playback intent.
  void OnOpenMediaAndroidIntent(const std::string& play_media_args_proto,
                                AndroidAppInfo app_info) {
    app_info.action = kIntentActionView;
    if (app_info.intent.empty()) {
      std::string url = GetAndroidIntentUrlFromMediaArgs(play_media_args_proto);
      if (!url.empty())
        app_info.intent = url;
    }
    VLOG(1) << "Playing android media";
    delegate().PlayAndroidMedia(std::move(app_info));
  }

  void OnOpenUrl(const std::string& play_media_args_proto) {
    std::string url = GetWebUrlFromMediaArgs(play_media_args_proto);
    // Fallback to web URL.
    if (!url.empty()) {
      VLOG(1) << "Playing web media";
      delegate().PlayWebMedia(url);
    }
  }

  void OnMediaControlAction(const std::string& action_name,
                            const std::string& media_action_args_proto) {
    if (action_name == kPauseTrackClientOp) {
      VLOG(1) << "Pausing media playback";
      delegate().Pause();
      return;
    }

    if (action_name == kResumeTrackClientOp) {
      VLOG(1) << "Resuming media playback";
      delegate().Resume();
      return;
    }

    if (action_name == kNextTrackClientOp) {
      VLOG(1) << "Playing next track";
      delegate().NextTrack();
      return;
    }

    if (action_name == kPrevTrackClientOp) {
      VLOG(1) << "Playing previous track";
      delegate().PreviousTrack();
      return;
    }

    if (action_name == kStopTrackClientOp) {
      VLOG(1) << "Stop media playback";
      delegate().Stop();
      return;
    }
  }

  mojom::MediaDelegate& delegate() { return *parent_->delegate_; }

  const raw_ptr<MediaController> parent_;
};

MediaController::MediaController()
    : events_observer_(std::make_unique<GrpcEventsObserver>(this)) {}

MediaController::~MediaController() = default;

void MediaController::Bind(
    mojo::PendingReceiver<mojom::MediaController> receiver,
    mojo::PendingRemote<mojom::MediaDelegate> delegate) {
  receiver_.Bind(std::move(receiver));
  delegate_.Bind(std::move(delegate));
}

void MediaController::ResumeInternalMediaPlayer() {
  VLOG(1) << "Resume internal media player";
  if (assistant_client_)
    assistant_client_->ResumeCurrentStream();
}

void MediaController::PauseInternalMediaPlayer() {
  VLOG(1) << "Pause internal media player";
  if (assistant_client_)
    assistant_client_->PauseCurrentStream();
}

void MediaController::SetExternalPlaybackState(mojom::MediaStatePtr state) {
  DCHECK(!state.is_null());
  VLOG(1) << "Update external playback state to " << state->playback_state;
  if (assistant_client_) {
    MediaStatus media_status_proto;
    ConvertMediaStatusToV2FromMojom(*state, &media_status_proto);
    assistant_client_->SetExternalPlaybackState(media_status_proto);
  }
}

void MediaController::OnAssistantClientRunning(
    AssistantClient* assistant_client) {
  assistant_client_ = assistant_client;
  // `events_observer_` outlives `assistant_client_`.
  assistant_client->AddDeviceStateEventObserver(events_observer_.get());
  assistant_client->AddMediaActionFallbackEventObserver(events_observer_.get());
}

void MediaController::SendGrpcMessageForTesting(
    const ::assistant::api::OnDeviceStateEventRequest& request) {
  events_observer_->OnGrpcMessage(request);
}

void MediaController::SendGrpcMessageForTesting(
    const ::assistant::api::OnMediaActionFallbackEventRequest& request) {
  events_observer_->OnGrpcMessage(request);
}

}  // namespace ash::libassistant
