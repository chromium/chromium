// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_H_

#include "base/containers/flat_set.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
struct VectorIcon;
}  // namespace gfx

namespace media_session {
struct MediaMetadata;
}  // namespace media_session

namespace media_message_center {

// MediaNotificationView will show up as a custom view. It will show the
// currently playing media and provide playback controls.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationView
    : public views::View {
  METADATA_HEADER(MediaNotificationView, views::View)

 public:
  // When |forced_expanded_state| has a value, the notification will be forced
  // into that expanded state and the user won't be given a button to toggle the
  // expanded state. Subsequent |SetExpanded()| calls will be ignored until
  // |SetForcedExpandedState(nullptr)| is called.
  virtual void SetForcedExpandedState(bool* forced_expanded_state) = 0;
  virtual void SetExpanded(bool expanded) = 0;

  virtual void UpdateCornerRadius(int top_radius, int bottom_radius) = 0;
  virtual void UpdateWithMediaSessionInfo(
      const media_session::mojom::MediaSessionInfoPtr& session_info) = 0;
  virtual void UpdateWithMediaMetadata(
      const media_session::MediaMetadata& metadata) = 0;
  virtual void UpdateWithMediaActions(
      const base::flat_set<media_session::mojom::MediaSessionAction>&
          actions) = 0;
  virtual void UpdateWithMediaPosition(
      const media_session::MediaPosition& position) = 0;
  virtual void UpdateWithMediaArtwork(const gfx::ImageSkia& image) = 0;
  virtual void UpdateWithChapterArtwork(int index,
                                        const gfx::ImageSkia& image) = 0;
  // Updates the background color to match that of the favicon.
  virtual void UpdateWithFavicon(const gfx::ImageSkia& icon) = 0;
  // Sets the icon to be displayed in the notification's header section.
  // |vector_icon| must outlive the MediaNotificationView.
  virtual void UpdateWithVectorIcon(const gfx::VectorIcon* vector_icon) = 0;
  // Called by MediaNotificationItem to update mute state.
  virtual void UpdateWithMuteStatus(bool mute) = 0;
  // Called by MediaNotificationitem to update volume.
  virtual void UpdateWithVolume(float volume) = 0;

  // Called to update MediaNotificationView based on whether the device selector
  // view has been expanded and devices are visible to users.
  virtual void UpdateDeviceSelectorVisibility(bool visible) = 0;

  // Called to update MediaNotificationView based on whether the device selector
  // view has available devices.
  virtual void UpdateDeviceSelectorAvailability(bool has_devices) = 0;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_VIEW_H_
