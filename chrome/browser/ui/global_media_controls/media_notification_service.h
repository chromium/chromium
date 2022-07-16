// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_producer.h"
#include "chrome/browser/ui/global_media_controls/media_item_ui_device_selector_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/global_media_controls/presentation_request_notification_producer.h"
#include "components/global_media_controls/public/media_session_item_producer.h"
#include "components/global_media_controls/public/media_session_item_producer_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class StartPresentationContext;
class WebContents;
}  // namespace content

namespace global_media_controls {
class MediaDialogDelegate;
class MediaItemManager;
}  // namespace global_media_controls

namespace media_router {
class CastDialogController;
}  // namespace media_router

class MediaNotificationService
    : public KeyedService,
      public MediaItemUIDeviceSelectorDelegate,
      public global_media_controls::MediaSessionItemProducerObserver {
 public:
  MediaNotificationService(Profile* profile, bool show_from_all_profiles);
  MediaNotificationService(const MediaNotificationService&) = delete;
  MediaNotificationService& operator=(const MediaNotificationService&) = delete;
  ~MediaNotificationService() override;

  // KeyedService implementation.
  void Shutdown() override;

  global_media_controls::MediaItemManager* media_item_manager() {
    return item_manager_.get();
  }

  // MediaItemUIDeviceSelectorDelegate:
  void OnAudioSinkChosen(const std::string& item_id,
                         const std::string& sink_id) override;
  base::CallbackListSubscription RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallback callback)
      override;
  base::CallbackListSubscription
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      const std::string& id,
      base::RepeatingCallback<void(bool)> callback) override;

  // global_media_controls::MediaSessionItemProducerObserver:
  void OnMediaSessionItemCreated(const std::string& id) override;
  void OnMediaSessionItemDestroyed(const std::string& id) override;
  void OnMediaSessionActionButtonPressed(
      const std::string& id,
      media_session::mojom::MediaSessionAction action) override;

  void SetDialogDelegateForWebContents(
      global_media_controls::MediaDialogDelegate* delegate,
      content::WebContents* contents);

  // True if there are active non-frozen media session notifications or active
  // cast notifications associated with |web_contents|.
  bool HasActiveNotificationsForWebContents(
      content::WebContents* web_contents) const;

  // True if there are local cast notifications.
  bool HasLocalCastNotifications() const;

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

  void set_device_provider_for_testing(
      std::unique_ptr<MediaNotificationDeviceProvider> device_provider);

 private:
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

  class PresentationManagerObservation
      : public media_router::WebContentsPresentationManager::Observer {
   public:
    PresentationManagerObservation(base::RepeatingClosure cast_started_callback,
                                   content::WebContents* web_contents);
    PresentationManagerObservation(const PresentationManagerObservation&) =
        delete;
    PresentationManagerObservation& operator=(
        const PresentationManagerObservation&) = delete;
    ~PresentationManagerObservation() override;

    // media_router::WebContentsPresentationManager::Observer:
    void OnMediaRoutesChanged(
        const std::vector<media_router::MediaRoute>& routes) override;

    void SetPresentationManagerForTesting(
        base::WeakPtr<media_router::WebContentsPresentationManager>
            presentation_manager);

   private:
    base::RepeatingClosure cast_started_callback_;
    base::WeakPtr<media_router::WebContentsPresentationManager>
        presentation_manager_;
  };

  // Called by PresentationManagerObservation when casting starts for its
  // WebContents.
  void OnCastStarted(content::WebContents* web_contents);

  // True if there are cast notifications associated with |web_contents|.
  bool HasCastNotificationsForWebContents(
      content::WebContents* web_contents) const;

  bool HasActiveControllableSessionForWebContents(
      content::WebContents* web_contents) const;

  std::string GetActiveControllableSessionForWebContents(
      content::WebContents* web_contents) const;

  std::unique_ptr<global_media_controls::MediaItemManager> item_manager_;

  std::unique_ptr<global_media_controls::MediaSessionItemProducer>
      media_session_item_producer_;
  std::unique_ptr<CastMediaNotificationProducer> cast_notification_producer_;
  std::unique_ptr<PresentationRequestNotificationProducer>
      presentation_request_notification_producer_;

  // Observes media_router::WebContentsPresentationManagers so we can dismiss
  // the dialog when casting starts.
  std::map<std::string, PresentationManagerObservation>
      presentation_manager_observations_;

  // Used to initialize a MediaRouterUI.
  std::unique_ptr<media_router::StartPresentationContext> context_;

  // Generates a list of available audio devices.
  std::unique_ptr<MediaNotificationDeviceProvider> device_provider_;

  // Tracks the number of times we have recorded an action for a specific
  // source. We use this to cap the number of UKM recordings per site.
  std::map<ukm::SourceId, int> actions_recorded_to_ukm_;

  base::WeakPtrFactory<MediaNotificationService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
