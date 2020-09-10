// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_MODERN_IMPL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_MODERN_IMPL_H_

#include "components/media_message_center/media_notification_view.h"

namespace media_message_center {
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationViewModernImpl
    : public MediaNotificationView {
 public:
  MediaNotificationViewModernImpl() = default;

  // MediaNotificationView
  void SetForcedExpandedState(bool* forced_expanded_state) override {}
  void SetExpanded(bool expanded) override {}
  void UpdateCornerRadius(int top_radius, int bottom_radius) override {}
  void UpdateWithMediaSessionInfo(
      const media_session::mojom::MediaSessionInfoPtr& session_info) override {}
  void UpdateWithMediaMetadata(
      const media_session::MediaMetadata& metadata) override {}
  void UpdateWithMediaActions(
      const base::flat_set<media_session::mojom::MediaSessionAction>& actions)
      override {}
  void UpdateWithMediaArtwork(const gfx::ImageSkia& image) override {}
  void UpdateWithFavicon(const gfx::ImageSkia& icon) override {}
  void UpdateWithVectorIcon(const gfx::VectorIcon& vector_icon) override {}
  void UpdateDeviceSelectorAvailability(bool availability) override {}
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_MODERN_IMPL_H_
