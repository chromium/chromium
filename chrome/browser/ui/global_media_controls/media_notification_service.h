// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_media_controls/cast_media_notification_producer.h"
#include "chrome/browser/ui/global_media_controls/media_item_ui_device_selector_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "chrome/browser/ui/global_media_controls/presentation_request_notification_producer.h"
#include "chrome/browser/ui/global_media_controls/supplemental_device_picker_producer.h"
#include "components/global_media_controls/public/media_session_item_producer.h"
#include "components/global_media_controls/public/media_session_item_producer_observer.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/presentation_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace content {
class WebContents;
}  // namespace content

namespace global_media_controls {
class MediaDialogDelegate;
class MediaItemManager;
}  // namespace global_media_controls

namespace media_router {
class CastDialogController;
class StartPresentationContext;
}  // namespace media_router

class MediaNotificationService
    : public KeyedService,
      public MediaItemUIDeviceSelectorDelegate,
      public global_media_controls::MediaSessionItemProducerObserver,
      public global_media_controls::mojom::DeviceService {
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
  void OnMediaRemotingRequested(const std::string& item_id) override;

  void OnSinksDiscovered(const std::string& item_id);

  // global_media_controls::MediaSessionItemProducerObserver:
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

  // global_media_controls::mojom::DeviceService:
  void GetDeviceListHostForSession(
      const std::string& session_id,
      mojo::PendingReceiver<global_media_controls::mojom::DeviceListHost>
          host_receiver,
      mojo::PendingRemote<global_media_controls::mojom::DeviceListClient>
          client_remote) override;
  void GetDeviceListHostForPresentation(
      mojo::PendingReceiver<global_media_controls::mojom::DeviceListHost>
          host_receiver,
      mojo::PendingRemote<global_media_controls::mojom::DeviceListClient>
          client_remote) override;
  void SetDevicePickerProvider(
      mojo::PendingRemote<global_media_controls::mojom::DevicePickerProvider>
          provider_remote) override;

#if BUILDFLAG(IS_CHROMEOS)
  // Show the Global Media Controls dialog in Ash.
  void ShowDialogAsh(
      std::unique_ptr<media_router::StartPresentationContext> context);
#endif  // BUILDFLAG(IS_CHROMEOS)

  bool should_show_cast_local_media_iph() const {
    return should_show_cast_local_media_iph_;
  }
  void set_device_provider_for_testing(
      std::unique_ptr<MediaNotificationDeviceProvider> device_provider);

 private:
  friend class MediaNotificationProviderImplTest;
  friend class MediaNotificationServiceTest;
  friend class MediaNotificationServiceCastTest;
  friend class MediaToolbarButtonControllerTest;
  FRIEND_TEST_ALL_PREFIXES(MediaNotificationServiceCastTest,
                           CreateCastDialogControllerWithRemotePlayback);

  // Instantiates a MediaRouterViewsUI object associated with the Session with
  // the given |session_id|.
  std::unique_ptr<media_router::CastDialogController>
  CreateCastDialogControllerForSession(const std::string& session_id);

  // Instantiates a MediaRouterViewsUI object associated with the
  // PresentationRequest that |presentation_request_notification_producer_|
  // manages.
  std::unique_ptr<media_router::CastDialogController>
  CreateCastDialogControllerForPresentationRequest();

  void CreateCastDeviceListHost(
      std::unique_ptr<media_router::CastDialogController> dialog_controller,
      mojo::PendingReceiver<global_media_controls::mojom::DeviceListHost>
          host_receiver,
      mojo::PendingRemote<global_media_controls::mojom::DeviceListClient>
          client_remote,
      std::optional<std::string> remoting_session_id);

  // True if there are cast notifications associated with |web_contents|.
  bool HasCastNotificationsForWebContents(
      content::WebContents* web_contents) const;

  // True if there is tab mirroring session associated with `web_contents`.
  bool HasTabMirroringSessionForWebContents(
      content::WebContents* web_contents) const;

  bool HasActiveControllableSessionForWebContents(
      content::WebContents* web_contents) const;

  std::string GetActiveControllableSessionForWebContents(
      content::WebContents* web_contents) const;

  void RemoveDeviceListHost(int host);

  const raw_ptr<Profile> profile_;

  std::unique_ptr<global_media_controls::MediaItemManager> item_manager_;

  std::unique_ptr<global_media_controls::MediaSessionItemProducer>
      media_session_item_producer_;
  std::unique_ptr<CastMediaNotificationProducer> cast_notification_producer_;
  std::unique_ptr<SupplementalDevicePickerProducer>
      supplemental_device_picker_producer_;
  std::unique_ptr<PresentationRequestNotificationProducer>
      presentation_request_notification_producer_;

  // Used to initialize a MediaRouterUI.
  std::unique_ptr<media_router::StartPresentationContext> context_;

  // Generates a list of available audio devices.
  std::unique_ptr<MediaNotificationDeviceProvider> device_provider_;

  // Tracks the number of times we have recorded an action for a specific
  // source. We use this to cap the number of UKM recordings per site.
  std::map<ukm::SourceId, int> actions_recorded_to_ukm_;

  mojo::Receiver<global_media_controls::mojom::DeviceService> receiver_;

  // Maps from hosts' IDs to hosts.
  std::map<
      int,
      mojo::SelfOwnedReceiverRef<global_media_controls::mojom::DeviceListHost>>
      host_receivers_;

  bool shutdown_has_started_ = false;

  // It's set to true when MediaNotificationService receives sink updates for a
  // local media.
  bool should_show_cast_local_media_iph_ = false;

  base::WeakPtrFactory<MediaNotificationService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
