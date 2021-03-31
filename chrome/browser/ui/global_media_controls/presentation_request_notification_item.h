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

class PresentationRequestNotificationItem final
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

  // media_message_center::MediaNotificationItem
  void Dismiss() final;

  base::WeakPtr<PresentationRequestNotificationItem> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  const std::string& id() const { return id_; }
  media_router::StartPresentationContext* context() const {
    return context_.get();
  }
  const content::PresentationRequest request() const { return request_; }

 private:
  // media_message_center::MediaNotificationItem
  void SetView(media_message_center::MediaNotificationView* view) final;
  void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) final;
  void SeekTo(base::TimeDelta time) final {}
  media_message_center::SourceType SourceType() override;

  const std::string id_;
  MediaNotificationService* const notification_service_;
  // It is possible that |context_| is nullptr when it is created for a default
  // presentation request.
  std::unique_ptr<media_router::StartPresentationContext> context_;
  const content::PresentationRequest request_;

  media_message_center::MediaNotificationView* view_ = nullptr;

  base::WeakPtrFactory<PresentationRequestNotificationItem> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_ITEM_H_
