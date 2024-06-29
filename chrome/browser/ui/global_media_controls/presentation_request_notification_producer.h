// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_PRODUCER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_PRODUCER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/global_media_controls/presentation_request_notification_item.h"
#include "components/global_media_controls/public/mojom/device_service.mojom.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "content/public/browser/presentation_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

// An object that creates and manages media notifications related to
// presentation requests by delegating to the implementer of
// global_media_controls::mojom::DevicePickerProvider that handles the UI.
// On Chrome OS, this object lives on the browser-side of the crosapi split.
// On other platforms, both this and DevicePickerProvider live within the same
// browser process.
//
// The purpose of this class is somewhat subtle.  When a page uses the Cast API
// or Presentation API to make itself castable, we want there to be a
// notification for it in the global media controls dialog.  Most of the time,
// there will already be a notification because the active tab will be playing
// media that causes a notification to be created, but in some circumstances
// (e.g. a YouTube page loaded with the audio muted), this class is the only
// mechanism that will cause a notification to be shown.
//
// This class can only ever manage one notification at a time.  The notification
// correponds either to the default presentation request created in the active
// tab, or to a non-default presentation request being started (typically by the
// user clicking a Cast button in the active tab).
//
// The notification managed by this object only exists when the media control
// dialog is showing.  This is to prevent presentation requests from causing the
// media control button to become visible when it would otherwise be hidden.
//
// Once a Cast/Presentation session has been created, this class is no longer
// involved; at that point CastMediaNotificationProducer become responsible for
// managing the notification for an active session.
class PresentationRequestNotificationProducer final
    : public content::PresentationObserver,
      public global_media_controls::mojom::DevicePickerObserver {
 public:
  // See the comments on the member fields corresponding to the parameters.
  PresentationRequestNotificationProducer(
      base::RepeatingCallback<bool(content::WebContents*)>
          has_active_notifications_callback,
      const base::UnguessableToken& source_id);
  PresentationRequestNotificationProducer(
      const PresentationRequestNotificationProducer&) = delete;
  PresentationRequestNotificationProducer& operator=(
      const PresentationRequestNotificationProducer&) = delete;
  ~PresentationRequestNotificationProducer() final;

  void OnStartPresentationContextCreated(
      std::unique_ptr<media_router::StartPresentationContext> context);
  // Returns the WebContents associated with the PresentationRequest that
  // `this` manages.
  content::WebContents* GetWebContents();
  base::WeakPtr<PresentationRequestNotificationItem> GetNotificationItem();

  void BindProvider(
      mojo::PendingRemote<global_media_controls::mojom::DevicePickerProvider>
          pending_provider);

  // global_media_controls::mojom::DevicePickerObserver:
  void OnMediaUIOpened() override;
  void OnMediaUIClosed() override;
  void OnMediaUIUpdated() override;
  void OnPickerDismissed() override;

  // content::PresentationObserver:
  void OnPresentationsChanged(bool has_presentation) override;
  void OnDefaultPresentationChanged(
      const content::PresentationRequest* presentation_request) override;

  void SetTestPresentationManager(
      base::WeakPtr<media_router::WebContentsPresentationManager>
          presentation_manager);

 private:
  friend class PresentationRequestNotificationProducerTest;
  class PresentationRequestWebContentsObserver;

  void CreateItemForPresentationRequest(
      const content::PresentationRequest& request,
      std::unique_ptr<media_router::StartPresentationContext> context);
  void DeleteItemForPresentationRequest(const std::string& message);

  // Returns true if there is an item, and the item is for a non-default
  // presentation request.
  bool HasItemForNonDefaultRequest() const {
    return item_ && !item_->is_default_presentation_request();
  }

  // Queries |notification_service_| for active sessions associated with the
  // WebContents that |this| manages and stores the value in |should_show_|.
  // Show or hide |item_| if the visibility changed.
  void ShowOrHideItem();

  mojo::Remote<global_media_controls::mojom::DevicePickerProvider> provider_;

  mojo::Receiver<global_media_controls::mojom::DevicePickerObserver>
      observer_receiver_;

  // An observer for the WebContents associated with |item_| that closes the
  // dialog when the WebContents is destroyed or navigated.
  std::unique_ptr<PresentationRequestWebContentsObserver>
      presentation_request_observer_;

  // This callback is called to determine if there currently are active
  // media notifications (not managed by `this`) that are associated with
  // `web_contents`.
  base::RepeatingCallback<bool(content::WebContents* web_contents)>
      has_active_notifications_callback_;

  // A copy of the WebContentsPresentationManager associated with the web
  // page where the media dialog is opened. The value is nullptr if the media
  // dialog is closed.
  // It is used to remove |this| from |presentation_manager_|'s observers.
  base::WeakPtr<media_router::WebContentsPresentationManager>
      presentation_manager_;
  //  A copy of the WebContentsPresentationManager used for testing.
  base::WeakPtr<media_router::WebContentsPresentationManager>
      test_presentation_manager_;

  // The notification managed by this producer, if there is one.
  std::optional<PresentationRequestNotificationItem> item_;

  // False if |notification_service_| should hide |item_| because there are
  // active notifications on WebContents managed by this producer.
  bool should_show_ = false;

  // Whether the media dialog containing the media notifications is open.
  bool is_dialog_open_ = false;

  // The MediaSession source ID for the current BrowserContext. This is passed
  // to `provider_` to distinguish `this` from other possible clients.
  const base::UnguessableToken source_id_;

  base::WeakPtrFactory<PresentationRequestNotificationProducer> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_PRODUCER_H_
