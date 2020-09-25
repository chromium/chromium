// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/presentation_request_notification_item.h"

#include "base/unguessable_token.h"
#include "chrome/browser/ui/global_media_controls/media_notification_service.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"

PresentationRequestNotificationItem::PresentationRequestNotificationItem(
    MediaNotificationService* notification_service,
    const content::PresentationRequest& request,
    std::unique_ptr<media_router::StartPresentationContext> context)
    : id_(base::UnguessableToken::Create().ToString()),
      notification_service_(notification_service),
      context_(std::move(context)) {
  // TODO(jrw): Save a copy of |request| once it is actually used.
  DCHECK(!context || request == context->presentation_request());
}

PresentationRequestNotificationItem::~PresentationRequestNotificationItem() {
  notification_service_->RemoveItem(id_);
}

void PresentationRequestNotificationItem::SetView(
    media_message_center::MediaNotificationView* view) {}

void PresentationRequestNotificationItem::OnMediaSessionActionButtonPressed(
    media_session::mojom::MediaSessionAction action) {}

void PresentationRequestNotificationItem::Dismiss() {}

bool PresentationRequestNotificationItem::SourceIsCast() {
  return false;
}
