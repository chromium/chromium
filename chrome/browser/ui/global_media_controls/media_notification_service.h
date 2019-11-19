// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_provider.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notifications_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/media_message_center/media_notification_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"

namespace content {
class WebContents;
}  // namespace content

namespace media_message_center {
class MediaSessionNotificationItem;
}  // namespace media_message_center

namespace service_manager {
class Connector;
}  // namespace service_manager

class MediaDialogDelegate;
class MediaNotificationContainerImpl;
class MediaNotificationServiceObserver;

class MediaNotificationService
    : public KeyedService,
      public media_session::mojom::AudioFocusObserver,
      public media_message_center::MediaNotificationController,
      public MediaNotificationContainerObserver {
 public:
  MediaNotificationService(Profile* profile,
                           service_manager::Connector* connector);
  MediaNotificationService(const MediaNotificationService&) = delete;
  MediaNotificationService& operator=(const MediaNotificationService&) = delete;
  ~MediaNotificationService() override;

  void AddObserver(MediaNotificationServiceObserver* observer);
  void RemoveObserver(MediaNotificationServiceObserver* observer);

  // media_session::mojom::AudioFocusObserver implementation.
  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;

  // media_message_center::MediaNotificationController implementation.
  void ShowNotification(const std::string& id) override;
  void HideNotification(const std::string& id) override;
  void RemoveItem(const std::string& id) override;
  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() const override;
  void LogMediaSessionActionButtonPressed(const std::string& id) override;

  // MediaNotificationContainerObserver implementation.
  void OnContainerExpanded(bool expanded) override {}
  void OnContainerMetadataChanged() override {}
  void OnContainerClicked(const std::string& id) override;
  void OnContainerDismissed(const std::string& id) override;
  void OnContainerDestroyed(const std::string& id) override;
  void OnContainerDraggedOut(const std::string& id, gfx::Rect bounds) override;

  // KeyedService implementation.
  void Shutdown() override;

  // Called by the |overlay_media_notifications_manager_| when an overlay
  // notification is closed.
  void OnOverlayNotificationClosed(const std::string& id);

  void OnCastNotificationsChanged();

  void SetDialogDelegate(MediaDialogDelegate* delegate);

  // True if there are active non-frozen media session notifications or active
  // cast notifications.
  bool HasActiveNotifications() const;

  // True if there are active frozen media session notifications.
  bool HasFrozenNotifications() const;

  // True if there is an open MediaDialogView associated with this service.
  bool HasOpenDialog() const;

 private:
  friend class MediaNotificationServiceTest;
  friend class MediaToolbarButtonControllerTest;

  class Session : public content::WebContentsObserver,
                  public media_session::mojom::MediaControllerObserver {
   public:
    Session(MediaNotificationService* owner,
            const std::string& id,
            std::unique_ptr<media_message_center::MediaSessionNotificationItem>
                item,
            content::WebContents* web_contents,
            mojo::Remote<media_session::mojom::MediaController> controller);
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session() override;

    // content::WebContentsObserver implementation.
    void WebContentsDestroyed() override;
    void OnWebContentsFocused(content::RenderWidgetHost*) override;

    // media_session::mojom::MediaControllerObserver:
    void MediaSessionInfoChanged(
        media_session::mojom::MediaSessionInfoPtr session_info) override;
    void MediaSessionMetadataChanged(
        const base::Optional<media_session::MediaMetadata>& metadata) override {
    }
    void MediaSessionActionsChanged(
        const std::vector<media_session::mojom::MediaSessionAction>& actions)
        override {}
    void MediaSessionChanged(
        const base::Optional<base::UnguessableToken>& request_id) override {}
    void MediaSessionPositionChanged(
        const base::Optional<media_session::MediaPosition>& position) override;

    media_message_center::MediaSessionNotificationItem* item() {
      return item_.get();
    }

    // Called when a new MediaController is given to the item. We need to
    // observe the same session as our underlying item.
    void SetController(
        mojo::Remote<media_session::mojom::MediaController> controller);

   private:
    void StartInactiveTimer();

    // Called when a session is interacted with (to reset |inactive_timer_|).
    void OnSessionInteractedWith();

    MediaNotificationService* owner_;
    const std::string id_;
    std::unique_ptr<media_message_center::MediaSessionNotificationItem> item_;

    // Used to stop/hide a paused session after a period of inactivity.
    base::OneShotTimer inactive_timer_;

    // Used to receive updates to the Media Session playback state.
    mojo::Receiver<media_session::mojom::MediaControllerObserver>
        observer_receiver_{this};
  };

  void OnReceivedAudioFocusRequests(
      std::vector<media_session::mojom::AudioFocusRequestStatePtr> sessions);

  base::WeakPtr<media_message_center::MediaNotificationItem>
  GetNotificationItem(const std::string& id);

  service_manager::Connector* const connector_;
  MediaDialogDelegate* dialog_delegate_ = nullptr;

  OverlayMediaNotificationsManager overlay_media_notifications_manager_;

  // Used to track whether there are any active controllable media sessions. If
  // not, then there's nothing to show in the dialog and we can hide the toolbar
  // icon.
  std::unordered_set<std::string> active_controllable_session_ids_;

  // Tracks the sessions that are currently frozen. If there are only frozen
  // sessions, we will disable the toolbar icon and wait to hide it.
  std::unordered_set<std::string> frozen_session_ids_;

  // Tracks the sessions that are currently dragged out of the dialog. These
  // should not be shown in the dialog and will be ignored for showing the
  // toolbar icon.
  std::unordered_set<std::string> dragged_out_session_ids_;

  // Stores a Session for each media session keyed by its |request_id| in string
  // format.
  std::map<std::string, Session> sessions_;

  // A map of all containers we're currently observing.
  std::map<std::string, MediaNotificationContainerImpl*> observed_containers_;

  // Connections with the media session service to listen for audio focus
  // updates and control media sessions.
  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote_;
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote_;
  mojo::Receiver<media_session::mojom::AudioFocusObserver>
      audio_focus_observer_receiver_{this};

  std::unique_ptr<CastMediaNotificationProvider> cast_notification_provider_;

  base::ObserverList<MediaNotificationServiceObserver> observers_;

  base::WeakPtrFactory<MediaNotificationService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
