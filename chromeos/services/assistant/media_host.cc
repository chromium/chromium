// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/media_host.h"

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromeos/assistant/internal/internal_util.h"
#include "chromeos/services/assistant/assistant_manager_service_impl.h"
#include "chromeos/services/assistant/media_session/assistant_media_session.h"
#include "chromeos/services/assistant/public/cpp/assistant_client.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "chromeos/services/assistant/public/cpp/migration/libassistant_v1_api.h"
#include "libassistant/shared/internal_api/assistant_manager_internal.h"
#include "libassistant/shared/public/assistant_manager.h"

// A macro which ensures we are running on the main thread.
#define ENSURE_MAIN_THREAD(method, ...)                                     \
  if (!main_task_runner_->RunsTasksInCurrentSequence()) {                   \
    main_task_runner_->PostTask(                                            \
        FROM_HERE,                                                          \
        base::BindOnce(method, weak_factory_.GetWeakPtr(), ##__VA_ARGS__)); \
    return;                                                                 \
  }

namespace chromeos {
namespace assistant {

namespace {
using assistant_client::MediaStatus;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;

constexpr char kNextTrackClientOp[] = "media.NEXT";
constexpr char kPauseTrackClientOp[] = "media.PAUSE";
constexpr char kPlayMediaClientOp[] = "media.PLAY_MEDIA";
constexpr char kPrevTrackClientOp[] = "media.PREVIOUS";
constexpr char kResumeTrackClientOp[] = "media.RESUME";
constexpr char kStopTrackClientOp[] = "media.STOP";

constexpr char kIntentActionView[] = "android.intent.action.VIEW";
}  // namespace

////////////////////////////////////////////////////////////////////////////////
//   MediaHost::ChromeosMediaStateObserver
////////////////////////////////////////////////////////////////////////////////

// Helper class that will observe media changes on ChromeOS and sync them to
// |MediaHost::UpdateMediaState| (which will sync them to
// Libassistant).
class MediaHost::ChromeosMediaStateObserver
    : private media_session::mojom::MediaControllerObserver {
 public:
  explicit ChromeosMediaStateObserver(MediaHost* parent) : parent_(parent) {
    DCHECK(parent_);
  }
  ChromeosMediaStateObserver(const ChromeosMediaStateObserver&) = delete;
  ChromeosMediaStateObserver& operator=(const ChromeosMediaStateObserver&) =
      delete;
  ~ChromeosMediaStateObserver() override = default;

  mojo::PendingRemote<MediaControllerObserver> BindNewPipeAndPassRemote() {
    return media_controller_observer_receiver_.BindNewPipeAndPassRemote();
  }

 private:
  // media_session::mojom::MediaControllerObserver overrides:
  void MediaSessionInfoChanged(
      media_session::mojom::MediaSessionInfoPtr info) override {
    media_session_info_ptr_ = std::move(info);
    UpdateMediaState();
  }
  void MediaSessionMetadataChanged(
      const base::Optional<media_session::MediaMetadata>& metadata) override {
    media_metadata_ = std::move(metadata);
    UpdateMediaState();
  }
  void MediaSessionActionsChanged(
      const std::vector<media_session::mojom::MediaSessionAction>& action)
      override {}
  void MediaSessionChanged(
      const base::Optional<base::UnguessableToken>& request_id) override {
    if (request_id.has_value())
      media_session_audio_focus_id_ = std::move(request_id.value());
  }
  void MediaSessionPositionChanged(
      const base::Optional<media_session::MediaPosition>& position) override {}

  void UpdateMediaState() {
    if (media_session_info_ptr_) {
      if (media_session_info_ptr_->is_sensitive) {
        // Do not update media state if the session is considered to be
        // sensitive (off the record profile).
        return;
      }

      if (media_session_info_ptr_->state ==
              MediaSessionInfo::SessionState::kSuspended &&
          media_session_info_ptr_->playback_state ==
              media_session::mojom::MediaPlaybackState::kPlaying) {
        // It is an intermediate state caused by some providers override the
        // playback state. We considered it as invalid and skip reporting the
        // state.
        return;
      }
    }

    MediaStatus media_status;

    // Set media metadata.
    if (media_metadata_.has_value()) {
      media_status.metadata.title =
          base::UTF16ToUTF8(media_metadata_.value().title);
    }

    // Set playback state.
    media_status.playback_state = MediaStatus::IDLE;
    if (media_session_info_ptr_ &&
        media_session_info_ptr_->state !=
            MediaSessionInfo::SessionState::kInactive) {
      switch (media_session_info_ptr_->playback_state) {
        case media_session::mojom::MediaPlaybackState::kPlaying:
          media_status.playback_state = MediaStatus::PLAYING;
          break;
        case media_session::mojom::MediaPlaybackState::kPaused:
          media_status.playback_state = MediaStatus::PAUSED;
          break;
      }
    }

    parent_->UpdateMediaState(media_session_audio_focus_id_, media_status);
  }

  MediaHost* const parent_;
  mojo::Receiver<media_session::mojom::MediaControllerObserver>
      media_controller_observer_receiver_{this};

  // Info associated to the active media session.
  media_session::mojom::MediaSessionInfoPtr media_session_info_ptr_;
  // The metadata for the active media session. It can be null to be reset,
  // e.g. the media that was being played has been stopped.
  base::Optional<media_session::MediaMetadata> media_metadata_ = base::nullopt;

  base::UnguessableToken media_session_audio_focus_id_ =
      base::UnguessableToken::Null();
};

////////////////////////////////////////////////////////////////////////////////
//   MediaHost::LibassistantMediaStateObserver
////////////////////////////////////////////////////////////////////////////////

// Helper class that will observe media changes in Libassisstant and sync them
// to either |MediaHost::interaction_subscribers_|,
// |MediaHost::media_controller_| or
// |MediaHost::media_session_|.
class MediaHost::LibassistantMediaStateObserver
    : public assistant_client::MediaManager::Listener {
 public:
  explicit LibassistantMediaStateObserver(
      MediaHost* parent,
      assistant_client::AssistantManagerInternal* assistant_manager_internal)
      : parent_(parent),
        main_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
    // Register handler for media actions.
    assistant_manager_internal->RegisterFallbackMediaHandler(
        [this](std::string action_name, std::string media_action_args_proto) {
          HandleMediaAction(action_name, media_action_args_proto);
        });
  }

  LibassistantMediaStateObserver(const LibassistantMediaStateObserver&) =
      delete;
  LibassistantMediaStateObserver& operator=(
      const LibassistantMediaStateObserver&) = delete;
  ~LibassistantMediaStateObserver() override = default;

 private:
  // Called from the Libassistant thread.
  void HandleMediaAction(std::string action_name,
                         std::string media_action_args_proto) {
    ENSURE_MAIN_THREAD(&LibassistantMediaStateObserver::HandleMediaAction,
                       action_name, media_action_args_proto);
    if (action_name == kPlayMediaClientOp)
      OnPlayMedia(media_action_args_proto);
    else
      OnMediaControlAction(action_name, media_action_args_proto);
  }

  void OnPlayMedia(const std::string& play_media_args_proto) {
    std::unique_ptr<AndroidAppInfo> app_info =
        GetAppInfoFromMediaArgs(play_media_args_proto);
    if (app_info) {
      OnOpenMediaAndroidIntent(play_media_args_proto, app_info.get());
    } else {
      std::string url = GetWebUrlFromMediaArgs(play_media_args_proto);
      // Fallback to web URL.
      if (!url.empty())
        OnOpenUrl(url, /*in_background=*/false);
    }
  }

  void OnOpenMediaAndroidIntent(
      const std::string& play_media_args_proto,
      AndroidAppInfo* app_info) {  // Handle android media playback intent.
    app_info->action = kIntentActionView;
    if (app_info->intent.empty()) {
      std::string url = GetAndroidIntentUrlFromMediaArgs(play_media_args_proto);
      if (!url.empty())
        app_info->intent = url;
    }
    for (auto& it : interaction_subscribers()) {
      bool success = it.OnOpenAppResponse(*app_info);
      HandleLaunchMediaIntentResponse(success);
    }
  }

  void HandleLaunchMediaIntentResponse(bool app_opened) {
    // TODO(llin): Handle the response.
    NOTIMPLEMENTED();
  }

  void OnOpenUrl(const std::string& url, bool in_background) {
    const GURL gurl = GURL(url);

    for (auto& it : interaction_subscribers())
      it.OnOpenUrlResponse(gurl, in_background);
  }

  void OnMediaControlAction(const std::string& action_name,
                            const std::string& media_action_args_proto) {
    if (action_name == kPauseTrackClientOp) {
      media_controller().Suspend();
      return;
    }

    if (action_name == kResumeTrackClientOp) {
      media_controller().Resume();
      return;
    }

    if (action_name == kNextTrackClientOp) {
      media_controller().NextTrack();
      return;
    }

    if (action_name == kPrevTrackClientOp) {
      media_controller().PreviousTrack();
      return;
    }

    if (action_name == kStopTrackClientOp) {
      media_controller().Suspend();
      return;
    }
    // TODO(llin): Handle media.SEEK_RELATIVE.
  }

  // assistant_client::MediaManager::Listener overrides:
  void OnPlaybackStateChange(
      const assistant_client::MediaStatus& status) override {
    ENSURE_MAIN_THREAD(
        &MediaHost::LibassistantMediaStateObserver::OnPlaybackStateChange,
        status);

    parent_->media_session_->NotifyMediaSessionMetadataChanged(status);
  }

  const base::ObserverList<AssistantInteractionSubscriber>&
  interaction_subscribers() {
    return *parent_->interaction_subscribers_;
  }

  media_session::mojom::MediaController& media_controller() {
    return *parent_->media_controller_;
  }

  MediaHost* const parent_;

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  base::WeakPtrFactory<LibassistantMediaStateObserver> weak_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
//   MediaHost
////////////////////////////////////////////////////////////////////////////////

MediaHost::MediaHost(AssistantClient* assistant_client,
                     const base::ObserverList<AssistantInteractionSubscriber>*
                         interaction_subscribers)
    : interaction_subscribers_(interaction_subscribers),
      media_session_(std::make_unique<AssistantMediaSession>(this)) {
  DCHECK(assistant_client);

  mojo::Remote<media_session::mojom::MediaControllerManager>
      media_controller_manager;
  assistant_client->RequestMediaControllerManager(
      media_controller_manager.BindNewPipeAndPassReceiver());
  media_controller_manager->CreateActiveMediaController(
      media_controller_.BindNewPipeAndPassReceiver());
}

MediaHost::~MediaHost() {
  Stop();
}

void MediaHost::Start(
    assistant_client::AssistantManagerInternal* assistant_manager_internal) {
  DCHECK(assistant_manager());

  libassistant_media_state_observer_ =
      std::make_unique<LibassistantMediaStateObserver>(
          this, assistant_manager_internal);

  if (media_manager())
    media_manager()->AddListener(libassistant_media_state_observer_.get());
}

void MediaHost::Stop() {
  // Note we do not reset |libassistant_media_state_observer_| here,
  // as there is no API to unregister it from Libassistant and the
  // Libassistant process might still be running right now.
  StopObservingMediaController();
}

void MediaHost::ResumeInternalMediaPlayer() {
  if (media_manager())
    media_manager()->Resume();
}

void MediaHost::PauseInternalMediaPlayer() {
  if (media_manager())
    media_manager()->Pause();
}

void MediaHost::SetRelatedInfoEnabled(bool enable) {
  if (enable) {
    StartObservingMediaController();
  } else {
    StopObservingMediaController();
    ResetMediaState();
  }
}

assistant_client::MediaManager* MediaHost::media_manager() {
  if (!assistant_manager())
    return nullptr;
  return assistant_manager()->GetMediaManager();
}

void MediaHost::UpdateMediaState(const base::UnguessableToken& media_session_id,
                                 const MediaStatus& media_status) {
  // MediaSession Integrated providers (include the libassistant internal
  // media provider) will trigger media state change event. Only update the
  // external media status if the state changes is triggered by external
  // providers.
  if (media_session_->internal_audio_focus_id() == media_session_id) {
    return;
  }

  if (media_manager())
    media_manager()->SetExternalPlaybackState(media_status);
}

void MediaHost::ResetMediaState() {
  if (media_manager()) {
    media_manager()->SetExternalPlaybackState(MediaStatus{});
  }
}

void MediaHost::StartObservingMediaController() {
  if (!features::IsMediaSessionIntegrationEnabled())
    return;

  if (chromeos_media_state_observer_)
    return;

  chromeos_media_state_observer_ =
      std::make_unique<ChromeosMediaStateObserver>(this);
  media_controller_->AddObserver(
      chromeos_media_state_observer_->BindNewPipeAndPassRemote());
}

void MediaHost::StopObservingMediaController() {
  chromeos_media_state_observer_.reset();
}

assistant_client::AssistantManager* MediaHost::assistant_manager() {
  auto* api = LibassistantV1Api::Get();
  return api ? api->assistant_manager() : nullptr;
}

}  // namespace assistant
}  // namespace chromeos
