// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_ITEM_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_ITEM_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service_observer.h"
#include "components/media_message_center/media_notification_item.h"
#include "content/public/browser/presentation_request.h"

class MediaNotificationService;

namespace media_router {
class StartPresentationContext;
}  // namespace media_router

class PresentationRequestNotificationItem
    : public media_message_center::MediaNotificationItem {
 public:
  PresentationRequestNotificationItem(
      MediaNotificationService* notification_service,
      const content::PresentationRequest& request,
      std::unique_ptr<media_router::StartPresentationContext> context);
  PresentationRequestNotificationItem(
      const PresentationRequestNotificationItem&) = delete;
  PresentationRequestNotificationItem& operator=(
      const PresentationRequestNotificationItem&) = delete;
  ~PresentationRequestNotificationItem() final;

  base::WeakPtr<PresentationRequestNotificationItem> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  const std::string& id() const { return id_; }
  media_router::StartPresentationContext* context() const {
    return context_.get();
  }

 private:
  // media_message_center::MediaNotificationItem
  void SetView(media_message_center::MediaNotificationView* view) final;
  void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) final;
  void Dismiss() final;
  bool SourceIsCast() final;

  const std::string id_;
  MediaNotificationService* const notification_service_;
  std::unique_ptr<media_router::StartPresentationContext> context_;
  base::WeakPtrFactory<PresentationRequestNotificationItem> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_ITEM_H_
