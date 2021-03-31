// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_NOTIFICATION_PRODUCER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_NOTIFICATION_PRODUCER_H_

#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer_set.h"
#include "chrome/browser/ui/global_media_controls/media_notification_producer.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_observer.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notification.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace media_message_center {
class MediaSessionNotificationItem;
}  // namespace media_message_center

class MediaNotificationContainerImpl;
class Profile;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class GlobalMediaControlsDismissReason {
  kUserDismissedNotification = 0,
  kInactiveTimeout = 1,
  kTabClosed = 2,
  kMediaSessionStopped = 3,
  kMaxValue = kMediaSessionStopped,
};

class MediaSessionNotificationProducer
    : public MediaNotificationProducer,
      public media_session::mojom::AudioFocusObserver,
      public MediaNotificationContainerObserver {
 public:
  MediaSessionNotificationProducer(MediaNotificationService* service,
                                   Profile* profile,
                                   bool show_from_all_profiles);
  ~MediaSessionNotificationProducer() override;

  // MediaNotificationProducer:
  base::WeakPtr<media_message_center::MediaNotificationItem>
  GetNotificationItem(const std::string& id) override;
  std::set<std::string> GetActiveControllableNotificationIds() const override;
  void OnItemShown(const std::string& id,
                   MediaNotificationContainerImpl* container) override;

  // media_session::mojom::AudioFocusObserver:
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;

  // MediaNotificationContainerObserver implementation.
  void OnContainerClicked(const std::string& id) override;
  void OnContainerDismissed(const std::string& id) override;
  void OnContainerDraggedOut(const std::string& id, gfx::Rect bounds) override;
  void OnAudioSinkChosen(const std::string& id,
                         const std::string& sink_id) override;

  void HideItem(const std::string& id);
  void RemoveItem(const std::string& id);
  // Puts the item with the given ID on the list of active items. Returns false
  // if we fail to do so because the item is hidden or is an overlay. Requires
  // that the item exists.
  bool ActivateItem(const std::string& id);
  bool HasSession(const std::string& id) const;
  bool IsSessionPlaying(const std::string& id) const;
  // Returns whether there still exists a session for |id|.
  bool OnOverlayNotificationClosed(const std::string& id);
  bool HasFrozenNotifications() const;
  std::unique_ptr<media_router::CastDialogController>
  CreateCastDialogControllerForSession(const std::string& id);
  bool HasActiveControllableSessionForWebContents(
      content::WebContents* web_contents) const;
  // Returns the notification id of the session associated with |web_contents|.
  // There is at most one session per WebContents.
  std::string GetActiveControllableSessionForWebContents(
      content::WebContents* web_contents) const;
  void LogMediaSessionActionButtonPressed(
      const std::string& id,
      media_session::mojom::MediaSessionAction action);

  // Used by a |MediaNotificationDeviceSelectorView| to query the system
  // for connected audio output devices.
  base::CallbackListSubscription RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallback callback);

  // Used by a |MediaNotificationAudioDeviceSelectorView| to become notified of
  // audio device switching capabilities. The callback will be immediately run
  // with the current availability.
  base::CallbackListSubscription
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      const std::string& id,
      base::RepeatingCallback<void(bool)> callback);

  void set_device_provider_for_testing(
      std::unique_ptr<MediaNotificationDeviceProvider> device_provider);

 private:
  friend class MediaNotificationServiceTest;
  friend class MediaToolbarButtonControllerTest;

  class Session
      : public content::WebContentsObserver,
        public media_session::mojom::MediaControllerObserver,
        public media_router::WebContentsPresentationManager::Observer {
   public:
    Session(MediaSessionNotificationProducer* owner,
            const std::string& id,
            std::unique_ptr<media_message_center::MediaSessionNotificationItem>
                item,
            content::WebContents* web_contents,
            mojo::Remote<media_session::mojom::MediaController> controller);
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() override;

    // content::WebContentsObserver:
    void WebContentsDestroyed() override;

    // media_session::mojom::MediaControllerObserver:
    void MediaSessionInfoChanged(
        media_session::mojom::MediaSessionInfoPtr session_info) override;
    void MediaSessionMetadataChanged(
        const base::Optional<media_session::MediaMetadata>& metadata) override {
    }
    void MediaSessionActionsChanged(
        const std::vector<media_session::mojom::MediaSessionAction>& actions)
        override;
    void MediaSessionChanged(
        const base::Optional<base::UnguessableToken>& request_id) override {}
    void MediaSessionPositionChanged(
        const base::Optional<media_session::MediaPosition>& position) override;

    // media_router::WebContentsPresentationManager::Observer:
    void OnMediaRoutesChanged(
        const std::vector<media_router::MediaRoute>& routes) override;

    media_message_center::MediaSessionNotificationItem* item() {
      return item_.get();
    }

    // Called when a new MediaController is given to the item. We need to
    // observe the same session as our underlying item.
    void SetController(
        mojo::Remote<media_session::mojom::MediaController> controller);

    // Sets the reason why this session was dismissed/removed. Can only be
    // called if the value has not already been set.
    void set_dismiss_reason(GlobalMediaControlsDismissReason reason);

    // Called when a session is interacted with (to reset |inactive_timer_|).
    void OnSessionInteractedWith();

    // Called when the notification associated with this session is pulled out
    // into an overlay or it's overlay is closed.
    void OnSessionOverlayStateChanged(bool is_in_overlay);

    bool IsPlaying() const;

    void SetAudioSinkId(const std::string& id);

    base::CallbackListSubscription
    RegisterIsAudioDeviceSwitchingSupportedCallback(
        base::RepeatingCallback<void(bool)> callback);

    void SetPresentationManagerForTesting(
        base::WeakPtr<media_router::WebContentsPresentationManager>
            presentation_manager);

   private:
    static void RecordDismissReason(GlobalMediaControlsDismissReason reason);

    void StartInactiveTimer();

    void OnInactiveTimerFired();

    void RecordInteractionDelayAfterPause();

    void MarkActiveIfNecessary();

    MediaSessionNotificationProducer* const owner_;
    const std::string id_;
    std::unique_ptr<media_message_center::MediaSessionNotificationItem> item_;

    // Used to stop/hide a paused session after a period of inactivity.
    base::OneShotTimer inactive_timer_;

    base::TimeTicks last_interaction_time_ = base::TimeTicks::Now();

    // The reason why this session was dismissed/removed.
    base::Optional<GlobalMediaControlsDismissReason> dismiss_reason_;

    // True if the session's playback state is "playing".
    bool is_playing_ = false;

    // True if we're currently marked inactive.
    bool is_marked_inactive_ = false;

    // True if we're in an overlay notification.
    bool is_in_overlay_ = false;

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

    base::WeakPtr<media_router::WebContentsPresentationManager>
        presentation_manager_;
  };

  // Looks up a Session object by its ID. Returns null if not found.
  Session* GetSession(const std::string& id);
  // Called by a Session when it becomes active.
  void OnSessionBecameActive(const std::string& id);
  // Called by a Session when it becomes inactive.
  void OnSessionBecameInactive(const std::string& id);
  void HideMediaDialog();
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

  // Tracks the sessions that are currently dragged out of the dialog. These
  // should not be shown in the dialog and will be ignored for showing the
  // toolbar icon.
  std::set<std::string> dragged_out_session_ids_;

  // Connections with the media session service to listen for audio focus
  // updates and control media sessions.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote_;
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote_;
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};

  MediaNotificationService* const service_;

  // Keeps track of all the containers we're currently observing.
  MediaNotificationContainerObserverSet container_observer_set_;

  OverlayMediaNotificationsManagerImpl overlay_media_notifications_manager_;

  // Stores a Session for each media session keyed by its |request_id| in string
  // format.
  std::map<std::string, Session> sessions_;

  // Tracks the number of times we have recorded an action for a specific
  // source. We use this to cap the number of UKM recordings per site.
  std::map<ukm::SourceId, int> actions_recorded_to_ukm_;

  std::unique_ptr<MediaNotificationDeviceProvider> device_provider_;

  base::WeakPtrFactory<MediaSessionNotificationProducer> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_NOTIFICATION_PRODUCER_H_
