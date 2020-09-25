// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_PROVIDER_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_PROVIDER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_observer.h"
#include "chrome/browser/ui/global_media_controls/presentation_request_notification_item.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"

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
// involved; at that point CastMediaNotificationProvider become responsible for
// managing the notification for an active session.
class PresentationRequestNotificationProvider
    : public media_router::WebContentsPresentationManager::Observer,
      public MediaNotificationServiceObserver {
 public:
  explicit PresentationRequestNotificationProvider(
      MediaNotificationService* notification_service);
  PresentationRequestNotificationProvider(
      const PresentationRequestNotificationProvider&) = delete;
  PresentationRequestNotificationProvider& operator=(
      const PresentationRequestNotificationProvider&) = delete;
  ~PresentationRequestNotificationProvider() final;

  base::WeakPtr<media_message_center::MediaNotificationItem>
  GetNotificationItem(const std::string& id);

  void OnStartPresentationContextCreated(
      std::unique_ptr<media_router::StartPresentationContext> context);

 private:
  // MediaNotificationServiceObserver
  void OnNotificationListChanged() final;
  void OnMediaDialogOpened() final;
  void OnMediaDialogClosed() final;

  void AfterMediaDialogOpened(
      base::WeakPtr<media_router::WebContentsPresentationManager>
          presentation_manager);
  void AfterMediaDialogClosed(
      base::WeakPtr<media_router::WebContentsPresentationManager>
          presentation_manager);

  // WebContentsPresentationManager::Observer
  void OnDefaultPresentationChanged(
      const content::PresentationRequest* presentation_request) final;

  void CreateItemForPresentationRequest(
      const content::PresentationRequest& request,
      std::unique_ptr<media_router::StartPresentationContext> context);

  // Returns true if there is an item, and the item is for a non-default
  // presentation request.
  bool HasItemForNonDefaultRequest() const { return item_ && item_->context(); }

  MediaNotificationService* const notification_service_;

  // The notification managed by this provider, if there is one.
  base::Optional<PresentationRequestNotificationItem> item_;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_PROVIDER_H_
