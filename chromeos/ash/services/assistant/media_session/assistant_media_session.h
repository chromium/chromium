// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/services/libassistant/public/mojom/media_controller.mojom-forward.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace ash::assistant {

class MediaHost;

// MediaSession manages the media session and audio focus for Assistant.
// MediaSession allows clients to observe its changes via MediaSessionObserver,
// and allows clients to resume/suspend/stop the managed players.
class COMPONENT_EXPORT(ASSISTANT_SERVICE) AssistantMediaSession
    : public media_session::mojom::MediaSession {
 public:
  explicit AssistantMediaSession(MediaHost* host);

  AssistantMediaSession(const AssistantMediaSession&) = delete;
  AssistantMediaSession& operator=(const AssistantMediaSession&) = delete;

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
  void SkipAd() override {}
  void PreviousSlide() override {}
  void NextSlide() override {}
  void Seek(base::TimeDelta seek_time) override {}
  void Stop(SuspendType suspend_type) override {}
  void GetMediaImageBitmap(const media_session::MediaImage& image,
                           int minimum_size_px,
                           int desired_size_px,
                           GetMediaImageBitmapCallback callback) override {}
  void SeekTo(base::TimeDelta seek_time) override {}
  void ScrubTo(base::TimeDelta seek_time) override {}
  void EnterPictureInPicture() override {}
  void ExitPictureInPicture() override {}
  void GetVisibility(GetVisibilityCallback callback) override;
  void SetAudioSinkId(const std::optional<std::string>& sink_id) override {}
  void ToggleMicrophone() override {}
  void ToggleCamera() override {}
  void HangUp() override {}
  void Raise() override {}
  void SetMute(bool mute) override {}
  void RequestMediaRemoting() override {}
  void EnterAutoPictureInPicture() override {}

  // Requests/abandons audio focus to the AudioFocusManager.
  void RequestAudioFocus(media_session::mojom::AudioFocusType audio_focus_type);
  void AbandonAudioFocusIfNeeded();

  void NotifyMediaSessionMetadataChanged(
      const libassistant::mojom::MediaState& status);

  base::WeakPtr<AssistantMediaSession> GetWeakPtr();

  // Returns if the session is currently active.
  bool IsSessionStateActive() const;
  // Returns if the session is currently ducking.
  bool IsSessionStateDucking() const;
  // Returns if the session is currently suspended.
  bool IsSessionStateSuspended() const;
  // Returns if the session is currently inactive.
  bool IsSessionStateInactive() const;

  // Returns internal audio focus id.
  base::UnguessableToken internal_audio_focus_id() {
    return internal_audio_focus_id_;
  }

  void SetInternalAudioFocusIdForTesting(const base::UnguessableToken& token);

 private:
  // Ensures that |audio_focus_ptr_| is connected.
  void EnsureServiceConnection();

  // Called by AudioFocusManager when an async audio focus request is completed.
  void FinishAudioFocusRequest(media_session::mojom::AudioFocusType type);
  void FinishInitialAudioFocusRequest(media_session::mojom::AudioFocusType type,
                                      const base::UnguessableToken& request_id);

  // Sets |session_info_|, |audio_focus_type_| and notifies observers about
  // the state change.
  void SetAudioFocusInfo(
      media_session::mojom::MediaSessionInfo::SessionState audio_focus_state,
      media_session::mojom::AudioFocusType audio_focus_type);

  // Notifies mojo observers that the MediaSessionInfo has changed.
  void NotifyMediaSessionInfoChanged();

  // The current metadata associated with the current media session.
  media_session::MediaMetadata metadata_;

  const raw_ptr<MediaHost> host_;

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  // Binding for Mojo pointer to |this| held by AudioFocusManager.
  mojo::Receiver<media_session::mojom::MediaSession> receiver_{this};

  mojo::RemoteSet<media_session::mojom::MediaSessionObserver> observers_;

  // Holds a pointer to the MediaSessionService.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_manager_;

  // If the media session has acquired audio focus then this will contain a
  // pointer to that requests AudioFocusRequestClient.
  mojo::Remote<media_session::mojom::AudioFocusRequestClient>
      audio_focus_request_client_;

  media_session::mojom::MediaSessionInfo session_info_;

  media_session::mojom::AudioFocusType audio_focus_type_;

  // Audio focus request Id for the internal media which is playing.
  base::UnguessableToken internal_audio_focus_id_ =
      base::UnguessableToken::Null();

  base::WeakPtrFactory<AssistantMediaSession> weak_factory_{this};
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_MEDIA_SESSION_ASSISTANT_MEDIA_SESSION_H_
