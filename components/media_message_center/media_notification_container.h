// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTAINER_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTAINER_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace media_message_center {

// MediaNotificationContainer is an interface for containers of
// MediaNotificationView components to receive events from the
// MediaNotificationView.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationContainer {
 public:
  // Called when MediaNotificationView's expanded state changes.
  virtual void OnExpanded(bool expanded) = 0;

  // Called when the MediaSessionInfo changes.
  virtual void OnMediaSessionInfoChanged(
      const media_session::mojom::MediaSessionInfoPtr& session_info) = 0;

  // Called when the metadata changes.
  virtual void OnMediaSessionMetadataChanged(
      const media_session::MediaMetadata& metadata) = 0;

  // Called when the set of visible MediaSessionActions changes.
  virtual void OnVisibleActionsChanged(
      const base::flat_set<media_session::mojom::MediaSessionAction>&
          actions) = 0;

  // Called when the media artwork changes.
  virtual void OnMediaArtworkChanged(const gfx::ImageSkia& image) = 0;

  // Called when MediaNotificationView's colors change.
  virtual void OnColorsChanged(SkColor foreground,
                               SkColor foreground_disabled,
                               SkColor background) = 0;

  // Called when the header row is clicked along with whether we want to
  // activate the web contents for the original media.
  virtual void OnHeaderClicked(bool activate_original_media = true) = 0;

  // Called when the start casting button is clicked on the quick settings media
  // view to request showing device list using device selector view in the quick
  // settings media detailed view.
  virtual void OnShowCastingDevicesRequested() {}

  // Called when the media list view (such as device selector view, chapter list
  // view and etc ) size has changed to request UI updates for view parents.
  virtual void OnListViewSizeChanged() {}

  // Called when a media action button in MediaNotificationView is pressed and
  // MediaNotificationContainer needs to handle the button event.
  virtual void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) {}

  // Called when a seek event is triggered in MediaNotificationView and
  // MediaNotificationContainer needs to handle the event.
  virtual void SeekTo(base::TimeDelta time) {}

 protected:
  virtual ~MediaNotificationContainer() = default;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTAINER_H_
