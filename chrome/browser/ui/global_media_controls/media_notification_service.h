// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_provider.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/global_media_controls/media_notification_producer.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notifications_manager_impl.h"
#include "chrome/browser/ui/global_media_controls/presentation_request_notification_provider.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/media_message_center/media_notification_controller.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "media/audio/audio_device_description.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom-forward.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class StartPresentationContext;
class WebContents;
}  // namespace content

namespace media_message_center {
class MediaSessionNotificationItem;
}  // namespace media_message_center

namespace media_router {
class CastDialogController;
}

class MediaDialogDelegate;
class MediaNotificationServiceObserver;
class MediaSessionNotificationProducer;

class MediaNotificationService
    : public KeyedService,
      public media_message_center::MediaNotificationController {
 public:
  MediaNotificationService(Profile* profile, bool show_from_all_profiles);
  MediaNotificationService(const MediaNotificationService&) = delete;
  MediaNotificationService& operator=(const MediaNotificationService&) = delete;
  ~MediaNotificationService() override;

  void AddObserver(MediaNotificationServiceObserver* observer);
  void RemoveObserver(MediaNotificationServiceObserver* observer);

  // media_message_center::MediaNotificationController implementation.
  void ShowNotification(const std::string& id) override;
  void HideNotification(const std::string& id) override;
  void RemoveItem(const std::string& id) override;
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override;
  void LogMediaSessionActionButtonPressed(
      const std::string& id,
      media_session::mojom::MediaSessionAction action) override;

  // KeyedService implementation.
  void Shutdown() override;

  // Adds a "suppplemental" notification, which should only be shown if there is
  // no other notification associated with the same web contents.
  void AddSupplementalNotification(const std::string& id,
                                   content::WebContents* web_contents);

  // Called by the |overlay_media_notifications_manager_| when an overlay
  // notification is closed.
  void OnOverlayNotificationClosed(const std::string& id);

  void OnCastNotificationsChanged();

  void SetDialogDelegate(MediaDialogDelegate* delegate);

  // Returns active controllable notifications gathered from all the
  // notification providers. If empty, then there's nothing to show in the
  // dialog and we can hide the toolbar icon.
  std::set<std::string> GetActiveControllableNotificationIds() const;

  // True if there are active non-frozen media session notifications or active
  // cast notifications.
  bool HasActiveNotifications() const;

  // True if there are active frozen media session notifications.
  bool HasFrozenNotifications() const;

  // True if there is an open MediaDialogView associated with this service.
  bool HasOpenDialog() const;

  void HideMediaDialog();

  std::unique_ptr<OverlayMediaNotification> PopOutNotification(
      const std::string& id,
      gfx::Rect bounds);

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

  void OnPresentationRequestCreated(
      std::unique_ptr<media_router::StartPresentationContext> context);

  void OnStartPresentationContextCreated(
      std::unique_ptr<media_router::StartPresentationContext> context);

  // Instantiates a MediaRouterViewsUI object associated with the Session with
  // the given |session_id|.
  std::unique_ptr<media_router::CastDialogController>
  CreateCastDialogControllerForSession(const std::string& session_id);

  void ShowAndObserveContainer(const std::string& id);

 private:
  // TODO(crbug.com/1021643): Remove this friend declaration once the Session
  // class is moved to MediaSessionNotificationProducer.
  friend class MediaSessionNotificationProducer;
  friend class MediaNotificationProviderImplTest;
  friend class MediaNotificationServiceTest;
  friend class MediaToolbarButtonControllerTest;
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           HideAfterTimeoutAndActiveAgainOnPlay);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           SessionIsRemovedImmediatelyWhenATabCloses);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest, DismissesMediaSession);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           HidesInactiveNotifications);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           HidingNotification_FeatureDisabled);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class GlobalMediaControlsDismissReason {
    kUserDismissedNotification = 0,
    kInactiveTimeout = 1,
    kTabClosed = 2,
    kMediaSessionStopped = 3,
    kMaxValue = kMediaSessionStopped,
  };

  // TODO(crbug.com/1021643): Move this class to
  // MediaSessionNotificationProducer.
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

  // Looks up a notification from any source.  Returns null if not found.
  base::WeakPtr<media_message_center::MediaNotificationItem>
  GetNotificationItem(const std::string& id);

  // Called after changing anything about a notification to notify any observers
  // and update the visibility of supplemental notifications.  If the change is
  // associated with a particular notification ID, that ID should be passed as
  // the argument, otherwise the argument should be nullptr.
  void OnNotificationChanged(const std::string* changed_notification_id);

  bool HasSessionForWebContents(content::WebContents* web_contents) const;

  MediaNotificationProducer* GetNotificationProducer(
      const std::string& notification_id);

  MediaDialogDelegate* dialog_delegate_ = nullptr;

  // A mapping of supplemental notification IDs to their associated web
  // contents.  See MediaNotificationController::AddSupplementalNotification for
  // a description of supplemental notifications.
  //
  // This map should usually have at most one item.
  base::flat_map<std::string, content::WebContents*>
      supplemental_notifications_;

  std::unique_ptr<PresentationRequestNotificationProvider>
      presentation_request_notification_provider_;
  std::unique_ptr<CastMediaNotificationProvider> cast_notification_provider_;
  std::unique_ptr<MediaSessionNotificationProducer>
      media_session_notification_producer_;

  // Pointers to all notification providers owned by |this|.
  std::set<MediaNotificationProducer*> notification_providers_;

  base::ObserverList<MediaNotificationServiceObserver> observers_;

  base::WeakPtrFactory<MediaNotificationService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
