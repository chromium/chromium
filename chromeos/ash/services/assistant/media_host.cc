// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/assistant/media_host.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/services/assistant/media_session/assistant_media_session.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_browser_delegate.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "chromeos/ash/services/libassistant/public/mojom/media_controller.mojom.h"
#include "chromeos/services/assistant/public/shared/utils.h"

namespace ash::assistant {

namespace {
using libassistant::mojom::PlaybackState;
using media_session::mojom::MediaSessionAction;
using media_session::mojom::MediaSessionInfo;
using media_session::mojom::MediaSessionInfoPtr;

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
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  // media_session::mojom::MediaControllerObserver overrides:
  void MediaSessionInfoChanged(MediaSessionInfoPtr info) override {
    media_session_info_ptr_ = std::move(info);
    UpdateMediaState();
  }
  void MediaSessionMetadataChanged(
      const std::optional<media_session::MediaMetadata>& metadata) override {
    media_metadata_ = std::move(metadata);
    UpdateMediaState();
  }
  void MediaSessionActionsChanged(
      const std::vector<MediaSessionAction>& action) override {}
  void MediaSessionChanged(
      const std::optional<base::UnguessableToken>& request_id) override {
    if (request_id.has_value())
      media_session_audio_focus_id_ = std::move(request_id.value());
  }
  void MediaSessionPositionChanged(
      const std::optional<media_session::MediaPosition>& position) override {}

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

    libassistant::mojom::MediaStatePtr media_state =
        libassistant::mojom::MediaState::New();
    media_state->metadata = libassistant::mojom::MediaMetadata::New();

    // Set media metadata.
    if (media_metadata_.has_value()) {
      media_state->metadata->title =
          base::UTF16ToUTF8(media_metadata_.value().title);
    }

    // Set playback state.
    media_state->playback_state = PlaybackState::kIdle;
    if (media_session_info_ptr_ &&
        media_session_info_ptr_->state !=
            MediaSessionInfo::SessionState::kInactive) {
      switch (media_session_info_ptr_->playback_state) {
        case media_session::mojom::MediaPlaybackState::kPlaying:
          media_state->playback_state = PlaybackState::kPlaying;
          break;
        case media_session::mojom::MediaPlaybackState::kPaused:
          media_state->playback_state = PlaybackState::kPaused;
          break;
      }
    }

    parent_->UpdateMediaState(media_session_audio_focus_id_,
                              std::move(media_state));
  }

  const raw_ptr<MediaHost> parent_;
  mojo::Receiver<media_session::mojom::MediaControllerObserver> receiver_{this};

  // Info associated to the active media session.
  MediaSessionInfoPtr media_session_info_ptr_;
  // The metadata for the active media session. It can be null to be reset,
  // e.g. the media that was being played has been stopped.
  std::optional<media_session::MediaMetadata> media_metadata_ = std::nullopt;

  base::UnguessableToken media_session_audio_focus_id_ =
      base::UnguessableToken::Null();
};

////////////////////////////////////////////////////////////////////////////////
//   MediaHost::LibassistantMediaStateObserver
////////////////////////////////////////////////////////////////////////////////

// Helper class that will observe media changes in Libassisstant and sync them
// to either |MediaHost::interaction_subscribers_|,
// |MediaHost::chromeos_media_controller_| or
// |MediaHost::media_session_|.
class MediaHost::LibassistantMediaDelegate
    : public libassistant::mojom::MediaDelegate {
 public:
  explicit LibassistantMediaDelegate(
      MediaHost* parent,
      mojo::PendingReceiver<MediaDelegate> pending_receiver)
      : parent_(parent), receiver_(this, std::move(pending_receiver)) {}

  LibassistantMediaDelegate(const LibassistantMediaDelegate&) = delete;
  LibassistantMediaDelegate& operator=(const LibassistantMediaDelegate&) =
      delete;
  ~LibassistantMediaDelegate() override = default;

 private:
  // libassistant::mojom::MediaDelegate implementation:
  void OnPlaybackStateChanged(
      libassistant::mojom::MediaStatePtr new_state) override {
    parent_->media_session_->NotifyMediaSessionMetadataChanged(*new_state);
  }

  void PlayAndroidMedia(const AndroidAppInfo& app_info) override {
    // This is the only action that can be executed when we play android media.
    DCHECK_EQ(app_info.action, kIntentActionView);
    // Status is meaningless when playing android media.
    DCHECK_EQ(app_info.status, AppStatus::kUnknown);

    for (auto& subscriber : interaction_subscribers())
      subscriber.OnOpenAppResponse(app_info);
  }

  void PlayWebMedia(const std::string& url) override {
    const GURL gurl = GURL(url);
    for (auto& it : interaction_subscribers())
      it.OnOpenUrlResponse(gurl, /*in_background=*/false);
  }

  void NextTrack() override { media_controller().NextTrack(); }

  void PreviousTrack() override { media_controller().PreviousTrack(); }

  void Pause() override { media_controller().Suspend(); }

  void Resume() override { media_controller().Resume(); }

  void Stop() override {
    // Note: we intentionally use 'suspend' here so the user can later resume;
    // if we issued 'stop' there would be no way to resume.
    // See b/140945356.
    media_controller().Suspend();
  }

  const base::ObserverList<AssistantInteractionSubscriber>&
  interaction_subscribers() {
    return *parent_->interaction_subscribers_;
  }

  media_session::mojom::MediaController& media_controller() {
    return *parent_->chromeos_media_controller_;
  }

  const raw_ptr<MediaHost> parent_;
  mojo::Receiver<MediaDelegate> receiver_;
};

////////////////////////////////////////////////////////////////////////////////
//   MediaHost
////////////////////////////////////////////////////////////////////////////////

MediaHost::MediaHost(AssistantBrowserDelegate* delegate,
                     const base::ObserverList<AssistantInteractionSubscriber>*
                         interaction_subscribers)
    : interaction_subscribers_(interaction_subscribers),
      media_session_(std::make_unique<AssistantMediaSession>(this)) {
  DCHECK(delegate);

  mojo::Remote<media_session::mojom::MediaControllerManager>
      media_controller_manager;
  delegate->RequestMediaControllerManager(
      media_controller_manager.BindNewPipeAndPassReceiver());
  media_controller_manager->CreateActiveMediaController(
      chromeos_media_controller_.BindNewPipeAndPassReceiver());
}

MediaHost::~MediaHost() = default;

void MediaHost::Initialize(
    libassistant::mojom::MediaController* libassistant_controller,
    mojo::PendingReceiver<libassistant::mojom::MediaDelegate> media_delegate) {
  DCHECK(!libassistant_media_controller_);

  libassistant_media_controller_ = libassistant_controller;
  libassistant_media_delegate_ = std::make_unique<LibassistantMediaDelegate>(
      this, std::move(media_delegate));
}

void MediaHost::Stop() {
  libassistant_media_controller_ = nullptr;
  StopObservingMediaController();
}

void MediaHost::ResumeInternalMediaPlayer() {
  if (!libassistant_media_controller_) {
    return;
  }
  libassistant_media_controller_->ResumeInternalMediaPlayer();
}

void MediaHost::PauseInternalMediaPlayer() {
  if (!libassistant_media_controller_) {
    return;
  }
  libassistant_media_controller_->PauseInternalMediaPlayer();
}

void MediaHost::SetRelatedInfoEnabled(bool enable) {
  if (enable) {
    StartObservingMediaController();
  } else {
    StopObservingMediaController();
    ResetMediaState();
  }
}

void MediaHost::UpdateMediaState(
    const base::UnguessableToken& media_session_id,
    libassistant::mojom::MediaStatePtr media_state) {
  // MediaSession Integrated providers (include the libassistant internal
  // media provider) will trigger media state change event. Only update the
  // external media status if the state changes is triggered by external
  // providers.
  if (media_session_->internal_audio_focus_id() == media_session_id) {
    return;
  }

  if (!libassistant_media_controller_) {
    return;
  }
  libassistant_media_controller_->SetExternalPlaybackState(
      std::move(media_state));
}

void MediaHost::ResetMediaState() {
  if (!libassistant_media_controller_) {
    return;
  }
  libassistant_media_controller_->SetExternalPlaybackState(
      libassistant::mojom::MediaState::New());
}

void MediaHost::StartObservingMediaController() {
  if (chromeos_media_state_observer_)
    return;

  chromeos_media_state_observer_ =
      std::make_unique<ChromeosMediaStateObserver>(this);
  chromeos_media_controller_->AddObserver(
      chromeos_media_state_observer_->BindNewPipeAndPassRemote());
}

void MediaHost::StopObservingMediaController() {
  chromeos_media_state_observer_.reset();
}

}  // namespace ash::assistant
