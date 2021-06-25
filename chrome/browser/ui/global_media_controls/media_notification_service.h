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
#include "chrome/browser/ui/global_media_controls/cast_media_notification_producer.h"
#include "chrome/browser/ui/global_media_controls/media_notification_container_observer.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/global_media_controls/media_notification_producer.h"
#include "chrome/browser/ui/global_media_controls/overlay_media_notifications_manager_impl.h"
#include "chrome/browser/ui/global_media_controls/presentation_request_notification_producer.h"
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

  // Called by the |overlay_media_notifications_manager_| when an overlay
  // notification is closed.
  void OnOverlayNotificationClosed(const std::string& id);

  void OnCastNotificationsChanged();

  // Called if the dialog is opened from the toolbar button. It shows all active
  // and controllable media notifications.
  void SetDialogDelegate(MediaDialogDelegate* delegate);
  // Called if the dialog is opened for a presentation request from |contents|.
  // It only shows media session notifications from |contents|.
  void SetDialogDelegateForWebContents(MediaDialogDelegate* delegate,
                                       content::WebContents* contents);

  // Returns active controllable notifications gathered from all the
  // notification producers. If empty, then there's nothing to show in the
  // dialog and we can hide the toolbar icon.
  std::set<std::string> GetActiveControllableNotificationIds() const;

  // True if there are active non-frozen media session notifications or active
  // cast notifications.
  bool HasActiveNotifications() const;
  // True if there are active non-frozen media session notifications or active
  // cast notifications associated with |web_contents|.
  bool HasActiveNotificationsForWebContents(
      content::WebContents* web_contents) const;

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
  // Instantiates a MediaRouterViewsUI object associated with the
  // PresentationRequest that |presentation_request_notification_producer_|
  // manages.
  std::unique_ptr<media_router::CastDialogController>
  CreateCastDialogControllerForPresentationRequest();

  void ShowAndObserveContainer(const std::string& id);

 private:
  // TODO(crbug.com/1021643): Remove this friend declaration once the Session
  // class is moved to MediaSessionNotificationProducer.
  friend class MediaSessionNotificationProducer;
  friend class MediaNotificationProviderImplTest;
  friend class MediaNotificationServiceTest;
  friend class MediaNotificationServiceCastTest;
  friend class MediaToolbarButtonControllerTest;
  friend class PresentationRequestNotificationProducerTest;

  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           HideAfterTimeoutAndActiveAgainOnPlay);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           SessionIsRemovedImmediatelyWhenATabCloses);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest, DismissesMediaSession);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           HidesInactiveNotifications);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceTest,
                           HidingNotification_FeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceCastTest,
                           ShowSupplementalNotifications);

  // Looks up a notification from any source.  Returns null if not found.
  base::WeakPtr<media_message_center::MediaNotificationItem>
  GetNotificationItem(const std::string& id);

  // Called after changing anything about a notification to notify any observers
  // and update the visibility of supplemental notifications.  If the change is
  // associated with a particular notification ID, that ID should be passed as
  // the argument, otherwise the argument should be nullptr.
  void OnNotificationChanged(const std::string* changed_notification_id);

  MediaNotificationProducer* GetNotificationProducer(
      const std::string& notification_id);

  // Updates |dialog_delegate_| and notifies |observers_|. Called from
  // SetDialogDelegate() and SetDialogDelegateForPresentationRequest().
  void SetDialogDelegateCommon(MediaDialogDelegate* delegate);

  MediaDialogDelegate* dialog_delegate_ = nullptr;

  std::unique_ptr<MediaSessionNotificationProducer>
      media_session_notification_producer_;
  std::unique_ptr<CastMediaNotificationProducer> cast_notification_producer_;
  std::unique_ptr<PresentationRequestNotificationProducer>
      presentation_request_notification_producer_;

  // Pointers to all notification producers owned by |this|.
  std::set<MediaNotificationProducer*> notification_producers_;

  base::ObserverList<MediaNotificationServiceObserver> observers_;

  base::WeakPtrFactory<MediaNotificationService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
