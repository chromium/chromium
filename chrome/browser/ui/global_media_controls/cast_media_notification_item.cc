// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/cast_media_notification_item.h"

#include "services/media_session/public/mojom/media_session.mojom.h"

namespace {

media_session::mojom::MediaSessionInfoPtr CreateSessionInfo() {
  auto session_info = media_session::mojom::MediaSessionInfo::New();
  session_info->state =
      media_session::mojom::MediaSessionInfo::SessionState::kSuspended;
  session_info->force_duck = false;
  session_info->playback_state =
      media_session::mojom::MediaPlaybackState::kPaused;
  session_info->is_controllable = true;
  session_info->prefer_stop_for_gain_focus_loss = false;
  return session_info;
}

}  // namespace

CastMediaNotificationItem::CastMediaNotificationItem(
    media_message_center::MediaNotificationController* notification_controller)
    : session_info_(CreateSessionInfo()) {}

CastMediaNotificationItem::~CastMediaNotificationItem() = default;

void CastMediaNotificationItem::SetView(
    media_message_center::MediaNotificationView* view) {
  view_ = view;
}

void CastMediaNotificationItem::OnMediaSessionActionButtonPressed(
    media_session::mojom::MediaSessionAction action) {
  // TODO(crbug.com/987479): Forward the action to the Cast receiver.
}

void CastMediaNotificationItem::Dismiss() {
  // TODO(crbug.com/987479): Hide the notification.
}

void CastMediaNotificationItem::OnMediaStatusUpdated(
    media_router::mojom::MediaStatusPtr status) {
  // TODO(crbug.com/987479): Update |session_info_| with the data from |status|,
  // and notify |view_|.
}
