// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_
#define CHROMEOS_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"
#include "libassistant/shared/public/media_manager.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace assistant_client {
struct MediaStatus;
}  // namespace assistant_client

namespace chromeos {
namespace assistant {

class AssistantManagerServiceImpl;

// MediaSession manages the media session and audio focus for Assistant.
// MediaSession allows clients to observe its changes via MediaSessionObserver,
// and allows clients to resume/suspend/stop the managed players.
class AssistantMediaSession : public media_session::mojom::MediaSession {
 public:
  enum class State { ACTIVE, SUSPENDED, INACTIVE };

  explicit AssistantMediaSession(
      mojom::Client* client,
      AssistantManagerServiceImpl* assistant_manager);
  ~AssistantMediaSession() override;

  // media_session.mojom.MediaSession overrides:
  void Suspend(SuspendType suspend_type) override;
  void Resume(SuspendType suspend_type) override;
  void StartDucking() override;
  void StopDucking() override;
  void GetMediaSessionInfo(GetMediaSessionInfoCallback callback) override;
  void GetDebugInfo(GetDebugInfoCallback callback) override;
  void AddObserver(
      mojo::PendingRemote<media_session::mojom::MediaSessionObserver> observer)
      override;
  void PreviousTrack() override {}
  void NextTrack() override {}
  void NotifyMediaSessionMetadataChanged(
      const assistant_client::MediaStatus& status);
  void SkipAd() override {}
  void Seek(base::TimeDelta seek_time) override {}
  void Stop(SuspendType suspend_type) override {}
  void GetMediaImageBitmap(const media_session::MediaImage& image,
                           int minimum_size_px,
                           int desired_size_px,
                           GetMediaImageBitmapCallback callback) override {}
  void SeekTo(base::TimeDelta seek_time) override {}
  void ScrubTo(base::TimeDelta seek_time) override {}

  // Requests/abandons audio focus to the AudioFocusManager.
  void RequestAudioFocus(media_session::mojom::AudioFocusType audio_focus_type);
  void AbandonAudioFocusIfNeeded();

  base::WeakPtr<AssistantMediaSession> GetWeakPtr();

  // Returns internal audio focus id.
  base::UnguessableToken internal_audio_focus_id() {
    return internal_audio_focus_id_;
  }

 private:
  // Ensures that |audio_focus_ptr_| is connected.
  void EnsureServiceConnection();

  // Called by AudioFocusManager when an async audio focus request is completed.
  void FinishAudioFocusRequest(media_session::mojom::AudioFocusType type);
  void FinishInitialAudioFocusRequest(media_session::mojom::AudioFocusType type,
                                      const base::UnguessableToken& request_id);

  // Returns information about |this|.
  media_session::mojom::MediaSessionInfoPtr GetMediaSessionInfoInternal();

  // Sets |audio_focus_state_|, |audio_focus_type_| and notifies observers about
  // the state change.
  void SetAudioFocusInfo(State audio_focus_state,
                         media_session::mojom::AudioFocusType audio_focus_type);

  // Notifies mojo observers that the MediaSessionInfo has changed.
  void NotifyMediaSessionInfoChanged();

  // Returns if the session is currently active.
  bool IsActive() const;

  // Returns if the session is currently suspended.
  bool IsSuspended() const;

  // The current metadata associated with the current media session.
  media_session::MediaMetadata metadata_;

  AssistantManagerServiceImpl* const assistant_manager_service_;
  mojom::Client* const client_;

  // Binding for Mojo pointer to |this| held by AudioFocusManager.
  mojo::Receiver<media_session::mojom::MediaSession> receiver_{this};

  assistant_client::TrackType current_track_;

  mojo::RemoteSet<media_session::mojom::MediaSessionObserver> observers_;

  // Holds a pointer to the MediaSessionService.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote_;

  // The ducking state of this media session. The initial value is |false|, and
  // is set to |true| after StartDucking(), and will be set to |false| after
  // StopDucking().
  bool is_ducking_ = false;

  // If the media session has acquired audio focus then this will contain a
  // pointer to that requests AudioFocusRequestClient.
  mojo::Remote<media_session::mojom::AudioFocusRequestClient>
      request_client_remote_;

  // The last updated |MediaSessionInfo| that was sent to |observers_|.
  media_session::mojom::MediaSessionInfoPtr session_info_;

  State audio_focus_state_ = State::INACTIVE;

  media_session::mojom::AudioFocusType audio_focus_type_;

  // Audio focus request Id for the internal media which is playing.
  base::UnguessableToken internal_audio_focus_id_ =
      base::UnguessableToken::Null();

  base::WeakPtrFactory<AssistantMediaSession> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AssistantMediaSession);
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_
