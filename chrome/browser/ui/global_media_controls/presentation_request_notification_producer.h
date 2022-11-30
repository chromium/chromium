// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_PRODUCER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_PRODUCER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_media_controls/presentation_request_notification_item.h"
#include "components/global_media_controls/public/media_item_manager_observer.h"
#include "components/global_media_controls/public/media_item_producer.h"
#include "components/global_media_controls/public/media_item_ui_observer.h"
#include "components/global_media_controls/public/media_item_ui_observer_set.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "content/public/browser/presentation_observer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace global_media_controls {
class MediaItemManager;
}  // namespace global_media_controls

class MediaNotificationService;

// An object that creates and manages media notifications related to
// presentation requests.
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
    : public global_media_controls::MediaItemProducer,
      public content::PresentationObserver,
      public global_media_controls::MediaItemManagerObserver,
      public global_media_controls::MediaItemUIObserver {
 public:
  explicit PresentationRequestNotificationProducer(
      MediaNotificationService* notification_service);
  PresentationRequestNotificationProducer(
      const PresentationRequestNotificationProducer&) = delete;
  PresentationRequestNotificationProducer& operator=(
      const PresentationRequestNotificationProducer&) = delete;
  ~PresentationRequestNotificationProducer() final;

  // global_media_controls::MediaItemProducer:
  base::WeakPtr<media_message_center::MediaNotificationItem> GetMediaItem(
      const std::string& id) override;
  // Returns the supplemental notification's id if it should be shown.
  std::set<std::string> GetActiveControllableItemIds() const override;
  bool HasFrozenItems() override;
  void OnItemShown(const std::string& id,
                   global_media_controls::MediaItemUI* item_ui) override;
  bool IsItemActivelyPlaying(const std::string& id) override;

  // global_media_controls::MediaItemUIObserver:
  void OnMediaItemUIDismissed(const std::string& id) override;

  void OnStartPresentationContextCreated(
      std::unique_ptr<media_router::StartPresentationContext> context);
  // Returns the WebContents associated with the PresentationRequest that
  // |this| manages.
  content::WebContents* GetWebContents();
  base::WeakPtr<PresentationRequestNotificationItem> GetNotificationItem();

  void SetTestPresentationManager(
      base::WeakPtr<media_router::WebContentsPresentationManager>
          presentation_manager);

 private:
  friend class PresentationRequestNotificationProducerTest;
  class PresentationRequestWebContentsObserver;

  // An observer for the WebContents associated with |item_| that closes the
  // dialog when the WebContents is destroyed or navigated.
  std::unique_ptr<PresentationRequestWebContentsObserver>
      presentation_request_observer_;

  // global_media_controls::MediaItemManagerObserver:
  void OnItemListChanged() final;
  void OnMediaDialogOpened() final;
  void OnMediaDialogClosed() final;

  void AfterMediaDialogOpened(
      base::WeakPtr<media_router::WebContentsPresentationManager>
          presentation_manager);
  void AfterMediaDialogClosed();

  // content::PresentationObserver:
  void OnPresentationsChanged(bool has_presentation) override;
  void OnDefaultPresentationChanged(
      const content::PresentationRequest* presentation_request) final;

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
  // WebContents that |this| manages and stores the value in |should_hide_|.
  // Show or hide |item_| if the visibility changed.
  void ShowOrHideItem();

  const raw_ptr<MediaNotificationService> notification_service_;

  const raw_ptr<global_media_controls::MediaItemManager> item_manager_;

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
  absl::optional<PresentationRequestNotificationItem> item_;

  // True if |notification_service_| should hide |item_| because there are
  // active notifications on WebContents managed by this producer.
  bool should_hide_ = true;

  global_media_controls::MediaItemUIObserverSet item_ui_observer_set_;

  base::WeakPtrFactory<PresentationRequestNotificationProducer> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_PRODUCER_H_
