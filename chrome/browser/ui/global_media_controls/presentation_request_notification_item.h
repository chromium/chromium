// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_ITEM_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_ITEM_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/media_message_center/media_notification_item.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "content/public/browser/presentation_request.h"

namespace global_media_controls {
class MediaItemManager;
}  // namespace global_media_controls

class PresentationRequestNotificationItem final
    : public media_message_center::MediaNotificationItem {
 public:
  PresentationRequestNotificationItem(
      global_media_controls::MediaItemManager* item_manager,
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
  bool is_default_presentation_request() const {
    return is_default_presentation_request_;
  }

  std::unique_ptr<media_router::StartPresentationContext> PassContext() {
    return std::move(context_);
  }
  const content::PresentationRequest request() const { return request_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(PresentationRequestNotificationItemTest,
                           NotificationHeader);

  // media_message_center::MediaNotificationItem
  void SetView(media_message_center::MediaNotificationView* view) final;
  void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) final;
  void SeekTo(base::TimeDelta time) final {}
  media_message_center::SourceType SourceType() override;
  void SetVolume(float volume) override {}
  void SetMute(bool mute) override {}

  const std::string id_;
  global_media_controls::MediaItemManager* const item_manager_;

  // True if the item is created from a default PresentationRequest, which means
  // |context_| is set to nullptr in the constructor.
  const bool is_default_presentation_request_;

  // |context_| is nullptr if:
  // (1) It is created for a default PresentationRequest;
  // (2) MediaNotificationService has passed |context_| to initialize a
  // CastDialogController.
  std::unique_ptr<media_router::StartPresentationContext> context_;
  const content::PresentationRequest request_;

  media_message_center::MediaNotificationView* view_ = nullptr;

  base::WeakPtrFactory<PresentationRequestNotificationItem> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_PRESENTATION_REQUEST_NOTIFICATION_ITEM_H_
