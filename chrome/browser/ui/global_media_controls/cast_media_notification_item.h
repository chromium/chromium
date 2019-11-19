// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_ITEM_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_ITEM_H_

#include "base/memory/weak_ptr.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojom/media_status.mojom.h"
#include "components/media_message_center/media_notification_item.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/media_session/public/cpp/media_metadata.h"

namespace media_message_center {
class MediaNotificationController;
}  // namespace media_message_center

// Represents the media notification shown in the Global Media Controls dialog
// for a Cast session. It is responsible for showing/hiding a
// MediaNotificationView.
class CastMediaNotificationItem
    : public media_message_center::MediaNotificationItem,
      public media_router::mojom::MediaStatusObserver {
 public:
  CastMediaNotificationItem(media_message_center::MediaNotificationController*
                                notification_controller);
  CastMediaNotificationItem(const CastMediaNotificationItem&) = delete;
  CastMediaNotificationItem& operator=(const CastMediaNotificationItem&) =
      delete;
  ~CastMediaNotificationItem() override;

  // media_message_center::MediaNotificationItem:
  void SetView(media_message_center::MediaNotificationView* view) override;
  void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) override;
  void Dismiss() override;

  // media_router::mojom::MediaStatusObserver:
  void OnMediaStatusUpdated(
      media_router::mojom::MediaStatusPtr status) override;

  base::WeakPtr<media_message_center::MediaNotificationItem> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  media_message_center::MediaNotificationView* view_ = nullptr;
  media_session::mojom::MediaSessionInfoPtr session_info_;
  base::WeakPtrFactory<CastMediaNotificationItem> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_ITEM_H_
