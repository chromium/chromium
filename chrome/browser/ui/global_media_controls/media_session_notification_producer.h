// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_NOTIFICATION_PRODUCER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_NOTIFICATION_PRODUCER_H_

#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/global_media_controls/media_session_notification_item.h"
#include "components/global_media_controls/public/media_item_manager_observer.h"
#include "components/global_media_controls/public/media_item_producer.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/global_media_controls/public/media_item_ui_observer_set.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class WebContents;
}  // namespace content

namespace global_media_controls {
class MediaItemManager;
class MediaItemUI;
}  // namespace global_media_controls

namespace media_router {
class CastDialogController;
class StartPresentationContext;
}  // namespace media_router

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
    : public global_media_controls::MediaItemProducer,
      public MediaSessionNotificationItem::Delegate,
      public media_session::mojom::AudioFocusObserver,
      public global_media_controls::MediaItemUIObserver {
 public:
  MediaSessionNotificationProducer(
      global_media_controls::MediaItemManager* item_manager,
      Profile* profile,
      bool show_from_all_profiles);
  ~MediaSessionNotificationProducer() override;

  // global_media_controls::MediaItemProducer:
  base::WeakPtr<media_message_center::MediaNotificationItem> GetMediaItem(
      const std::string& id) override;
  std::set<std::string> GetActiveControllableItemIds() override;
  bool HasFrozenItems() override;
  void OnItemShown(const std::string& id,
                   global_media_controls::MediaItemUI* item_ui) override;
  bool IsItemActivelyPlaying(const std::string& id) override;

  // MediaSessionNotificationItem::Delegate:
  void ActivateItem(const std::string& id) override;
  void HideItem(const std::string& id) override;
  void RemoveItem(const std::string& id) override;
  void LogMediaSessionActionButtonPressed(
      const std::string& id,
      media_session::mojom::MediaSessionAction action) override;

  // media_session::mojom::AudioFocusObserver:
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnRequestIdReleased(const base::UnguessableToken& request_id) override;

  // global_media_controls::MediaItemUIObserver implementation.
  void OnMediaItemUIClicked(const std::string& id) override;
  void OnMediaItemUIDismissed(const std::string& id) override;
  void OnAudioSinkChosen(const std::string& id,
                         const std::string& sink_id) override;

  bool HasSession(const std::string& id) const;
  std::unique_ptr<media_router::CastDialogController>
  CreateCastDialogControllerForSession(const std::string& id);
  bool HasActiveControllableSessionForWebContents(
      content::WebContents* web_contents) const;
  // Returns the notification id of the session associated with |web_contents|.
  // There is at most one session per WebContents.
  std::string GetActiveControllableSessionForWebContents(
      content::WebContents* web_contents) const;

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

  void OnStartPresentationContextCreated(
      std::unique_ptr<media_router::StartPresentationContext> context);

  void set_device_provider_for_testing(
      std::unique_ptr<MediaNotificationDeviceProvider> device_provider);

 private:
  friend class MediaNotificationServiceTest;
  friend class MediaSessionNotificationProducerTest;

  class Session
      : public media_session::mojom::MediaControllerObserver,
        public media_router::WebContentsPresentationManager::Observer {
   public:
    Session(MediaSessionNotificationProducer* owner,
            const std::string& id,
            std::unique_ptr<MediaSessionNotificationItem> item,
            content::WebContents* web_contents,
            mojo::Remote<media_session::mojom::MediaController> controller);
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() override;

    // media_session::mojom::MediaControllerObserver:
    void MediaSessionInfoChanged(
        media_session::mojom::MediaSessionInfoPtr session_info) override;
    void MediaSessionMetadataChanged(
        const absl::optional<media_session::MediaMetadata>& metadata) override {
    }
    void MediaSessionActionsChanged(
        const std::vector<media_session::mojom::MediaSessionAction>& actions)
        override;
    void MediaSessionChanged(
        const absl::optional<base::UnguessableToken>& request_id) override {}
    void MediaSessionPositionChanged(
        const absl::optional<media_session::MediaPosition>& position) override;

    // media_router::WebContentsPresentationManager::Observer:
    void OnMediaRoutesChanged(
        const std::vector<media_router::MediaRoute>& routes) override;

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

    content::WebContents* web_contents() const { return web_contents_; }

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
    std::unique_ptr<MediaSessionNotificationItem> item_;

    // Used to stop/hide a paused session after a period of inactivity.
    base::OneShotTimer inactive_timer_;

    base::TimeTicks last_interaction_time_ = base::TimeTicks::Now();

    // The reason why this session was dismissed/removed.
    absl::optional<GlobalMediaControlsDismissReason> dismiss_reason_;

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

    content::WebContents* const web_contents_;

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

  // Connections with the media session service to listen for audio focus
  // updates and control media sessions.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote_;
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote_;
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};

  global_media_controls::MediaItemManager* const item_manager_;

  // Keeps track of all the items we're currently observing.
  global_media_controls::MediaItemUIObserverSet item_ui_observer_set_;

  // Stores a Session for each media session keyed by its |request_id| in string
  // format.
  std::map<std::string, Session> sessions_;

  // Tracks the number of times we have recorded an action for a specific
  // source. We use this to cap the number of UKM recordings per site.
  std::map<ukm::SourceId, int> actions_recorded_to_ukm_;

  std::unique_ptr<MediaNotificationDeviceProvider> device_provider_;

  // Used to initialize a MediaRouterUI.
  std::unique_ptr<media_router::StartPresentationContext> context_;
  base::WeakPtrFactory<MediaSessionNotificationProducer> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_SESSION_NOTIFICATION_PRODUCER_H_
