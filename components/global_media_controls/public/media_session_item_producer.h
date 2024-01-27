// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_SESSION_ITEM_PRODUCER_H_
#define COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_SESSION_ITEM_PRODUCER_H_

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/global_media_controls/public/media_item_manager_observer.h"
#include "components/global_media_controls/public/media_item_producer.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/global_media_controls/public/media_item_ui_observer_set.h"
#include "components/global_media_controls/public/media_session_notification_item.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace global_media_controls {

class MediaItemManager;
class MediaItemUI;
class MediaSessionItemProducerObserver;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GlobalMediaControlsDismissReason {
  kUserDismissedNotification = 0,
  kInactiveTimeout = 1,
  kTabClosed = 2,
  kMediaSessionStopped = 3,
  kMaxValue = kMediaSessionStopped,
};

class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) MediaSessionItemProducer
    : public MediaItemProducer,
      public MediaSessionNotificationItem::Delegate,
      public media_session::mojom::AudioFocusObserver,
      public MediaItemUIObserver {
 public:
  // When given a |source_id|, the MediaSessionItemProducer will only
  // produce MediaItems for the given source (i.e. profile). When empty, it will
  // produce MediaItems for the Media Sessions on all sources (profiles).
  MediaSessionItemProducer(
      mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote,
      mojo::Remote<media_session::mojom::MediaControllerManager>
          controller_manager_remote,
      MediaItemManager* item_manager,
      std::optional<base::UnguessableToken> source_id);
  MediaSessionItemProducer(const MediaSessionItemProducer&) = delete;
  MediaSessionItemProducer& operator=(const MediaSessionItemProducer&) = delete;
  ~MediaSessionItemProducer() override;

  // MediaItemProducer:
  base::WeakPtr<media_message_center::MediaNotificationItem> GetMediaItem(
      const std::string& id) override;
  std::set<std::string> GetActiveControllableItemIds() const override;
  bool HasFrozenItems() override;
  void OnItemShown(const std::string& id, MediaItemUI* item_ui) override;
  bool IsItemActivelyPlaying(const std::string& id) override;

  // MediaSessionNotificationItem::Delegate:
  void ActivateItem(const std::string& id) override;
  void HideItem(const std::string& id) override;
  void RemoveItem(const std::string& id) override;
  void RefreshItem(const std::string& id) override;
  void LogMediaSessionActionButtonPressed(
      const std::string& id,
      media_session::mojom::MediaSessionAction action) override;

  // media_session::mojom::AudioFocusObserver:
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnRequestIdReleased(const base::UnguessableToken& request_id) override;

  // MediaItemUIObserver implementation.
  void OnMediaItemUIClicked(const std::string& id,
                            bool activate_original_media) override;
  void OnMediaItemUIDismissed(const std::string& id) override;

  void AddObserver(MediaSessionItemProducerObserver* observer);
  void RemoveObserver(MediaSessionItemProducerObserver* observer);

  bool HasSession(const std::string& id) const;

  void SetAudioSinkId(const std::string& id, const std::string& sink_id);

  media_session::mojom::RemotePlaybackMetadataPtr
  GetRemotePlaybackMetadataFromItem(const std::string& id);

  base::CallbackListSubscription
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      const std::string& id,
      base::RepeatingCallback<void(bool)> callback);

  // Called when a media session item is associated with a presentation request
  // as to show the origin associated with the request rather than that for the
  // top frame.
  void UpdateMediaItemSourceOrigin(const std::string& id,
                                   const url::Origin& origin);

 private:
  friend class MediaSessionItemProducerTest;

  class COMPONENT_EXPORT(GLOBAL_MEDIA_CONTROLS) Session
      : public media_session::mojom::MediaControllerObserver {
   public:
    Session(MediaSessionItemProducer* owner,
            const std::string& id,
            std::unique_ptr<MediaSessionNotificationItem> item,
            mojo::Remote<media_session::mojom::MediaController> controller);
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() override;

    // media_session::mojom::MediaControllerObserver:
    void MediaSessionInfoChanged(
        media_session::mojom::MediaSessionInfoPtr session_info) override;
    void MediaSessionMetadataChanged(
        const std::optional<media_session::MediaMetadata>& metadata) override {}
    void MediaSessionActionsChanged(
        const std::vector<media_session::mojom::MediaSessionAction>& actions)
        override;
    void MediaSessionChanged(
        const std::optional<base::UnguessableToken>& request_id) override {}
    void MediaSessionPositionChanged(
        const std::optional<media_session::MediaPosition>& position) override;

    // Called when the request ID associated with this session is released (i.e.
    // when the tab is closed).
    void OnRequestIdReleased();

    MediaSessionNotificationItem* item() { return item_.get(); }

    // Called when a new MediaController is given to the item. We need to
    // observe the same session as our underlying item.
    void SetController(
        mojo::Remote<media_session::mojom::MediaController> controller);

    // Sets the reason why this session was dismissed/removed. Can only be
    // called if the value has not already been set.
    void set_dismiss_reason(GlobalMediaControlsDismissReason reason);

    // Called when a session is interacted with (to reset |inactive_timer_|).
    void OnSessionInteractedWith();

    bool IsPlaying() const;

    void SetAudioSinkId(const std::string& id);

    base::CallbackListSubscription
    RegisterIsAudioDeviceSwitchingSupportedCallback(
        base::RepeatingCallback<void(bool)> callback);

   private:
    static void RecordDismissReason(GlobalMediaControlsDismissReason reason);

    void StartInactiveTimer();

    void OnInactiveTimerFired();

    void RecordInteractionDelayAfterPause();

    void MarkActiveIfNecessary();

    const raw_ptr<MediaSessionItemProducer> owner_;
    const std::string id_;
    std::unique_ptr<MediaSessionNotificationItem> item_;

    // Used to stop/hide a paused session after a period of inactivity.
    base::OneShotTimer inactive_timer_;

    base::TimeTicks last_interaction_time_ = base::TimeTicks::Now();

    // The reason why this session was dismissed/removed.
    std::optional<GlobalMediaControlsDismissReason> dismiss_reason_;

    // True if the session's playback state is "playing".
    bool is_playing_ = false;

    // True if we're currently marked inactive.
    bool is_marked_inactive_ = false;

    // True if the audio output device can be switched.
    bool is_audio_device_switching_supported_ = true;

    // Used to notify changes in audio output device switching capabilities.
    base::RepeatingCallbackList<void(bool)>
        is_audio_device_switching_supported_callback_list_;

    // Used to receive updates to the Media Session playback state.
    mojo::Receiver<media_session::mojom::MediaControllerObserver>
        observer_receiver_{this};

    // Used to request audio output be routed to a different device.
    mojo::Remote<media_session::mojom::MediaController> controller_;
  };

  // Looks up a Session object by its ID. Returns null if not found.
  Session* GetSession(const std::string& id);
  // Called by a Session when it becomes active.
  void OnSessionBecameActive(const std::string& id);
  // Called by a Session when it becomes inactive.
  void OnSessionBecameInactive(const std::string& id);
  void OnReceivedAudioFocusRequests(
      std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions);
  void OnItemUnfrozen(const std::string& id);

  // Used to track whether there are any active controllable sessions.
  std::set<std::string> active_controllable_session_ids_;

  // Tracks the sessions that are currently frozen. If there are only frozen
  // sessions, we will disable the toolbar icon and wait to hide it.
  std::set<std::string> frozen_session_ids_;

  // Tracks the sessions that are currently inactive. Sessions become inactive
  // after a period of time of being paused with no user interaction. Inactive
  // sessions are hidden from the dialog until the user interacts with them
  // again (e.g. by playing the session).
  std::set<std::string> inactive_session_ids_;

  // Connections with the media session service to listen for audio focus
  // updates and control media sessions.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote_;
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote_;
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};

  const raw_ptr<MediaItemManager> item_manager_;

  // Keeps track of all the items we're currently observing.
  MediaItemUIObserverSet item_ui_observer_set_;

  // Stores a Session for each media session keyed by its |request_id| in string
  // format.
  std::map<std::string, Session> sessions_;

  base::ObserverList<MediaSessionItemProducerObserver> observers_;

  base::WeakPtrFactory<MediaSessionItemProducer> weak_ptr_factory_{this};
};

}  // namespace global_media_controls

#endif  // COMPONENTS_GLOBAL_MEDIA_CONTROLS_PUBLIC_MEDIA_SESSION_ITEM_PRODUCER_H_
