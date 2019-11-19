// Copyright 2019 The Chromium Authors. All rights reserved.
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
  virtual void OnMediaSessionMetadataChanged() = 0;

  // Called when the set of visible MediaSessionActions changes.
  virtual void OnVisibleActionsChanged(
      const base::flat_set<media_session::mojom::MediaSessionAction>&
          actions) = 0;

  // Called when the media artwork changes.
  virtual void OnMediaArtworkChanged(const gfx::ImageSkia& image) = 0;

  // Called when MediaNotificationView's colors change.
  virtual void OnColorsChanged(SkColor foreground, SkColor background) = 0;

  // Called when the header row is clicked.
  virtual void OnHeaderClicked() = 0;

 protected:
  virtual ~MediaNotificationContainer() = default;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONTAINER_H_
