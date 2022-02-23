// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_MEDIA_HOST_H_
#define CHROMEOS_SERVICES_ASSISTANT_MEDIA_HOST_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "chromeos/services/libassistant/public/mojom/media_controller.mojom-forward.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace chromeos {
namespace assistant {

class AssistantBrowserDelegate;
class AssistantInteractionSubscriber;
class AssistantManagerServiceImpl;
class AssistantMediaSession;

// Handles all media related interactions with Libassistant, which can broadly
// be separated in 2 responsibilities:
//   1) Let Libassistant know about the currently playing media.
//   1) Let Libassistant control media (start/stop/open spotify).
class COMPONENT_EXPORT(ASSISTANT_SERVICE) MediaHost {
 public:
  MediaHost(AssistantBrowserDelegate* delegate,
            const base::ObserverList<AssistantInteractionSubscriber>*
                interaction_subscribers);
  MediaHost(const MediaHost&) = delete;
  MediaHost& operator=(const MediaHost&) = delete;
  ~MediaHost();

  void Initialize(
      chromeos::libassistant::mojom::MediaController* libassistant_controller,
      mojo::PendingReceiver<chromeos::libassistant::mojom::MediaDelegate>
          media_delegate);

  // Stop observing ChromeOS media state.
  void Stop();

  // Pause/resume playback of Libassistant media player (which plays podcasts
  // and news).
  void ResumeInternalMediaPlayer();
  void PauseInternalMediaPlayer();

  // Called when the user allows/disallows related info.
  // We only observe the current playing audio when related info is allowed.
  void SetRelatedInfoEnabled(bool enable);

  AssistantMediaSession& media_session() { return *media_session_; }

 private:
  class LibassistantMediaDelegate;
  class ChromeosMediaStateObserver;

  chromeos::libassistant::mojom::MediaController&
  libassistant_media_controller();

  void UpdateMediaState(
      const base::UnguessableToken& media_session_id,
      chromeos::libassistant::mojom::MediaStatePtr media_state);
  void ResetMediaState();

  void StartObservingMediaController();
  void StopObservingMediaController();

  // Owned by our parent |AssistantManagerServiceImpl|.
  const base::ObserverList<AssistantInteractionSubscriber>* const
      interaction_subscribers_;
  // Owned by our parent |AssistantManagerServiceImpl|.
  chromeos::libassistant::mojom::MediaController*
      libassistant_media_controller_ = nullptr;

  std::unique_ptr<AssistantMediaSession> media_session_;
  mojo::Remote<media_session::mojom::MediaController>
      chromeos_media_controller_;

  // Helper class that will observe media changes on ChromeOS and sync them
  // to Libassistant.
  std::unique_ptr<ChromeosMediaStateObserver> chromeos_media_state_observer_;
  // Helper class that will observe media changes in Libassistant and
  // sync/apply them in ChromeOS.
  std::unique_ptr<LibassistantMediaDelegate> libassistant_media_delegate_;
};

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_MEDIA_HOST_H_
