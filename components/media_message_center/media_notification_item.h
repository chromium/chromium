// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ITEM_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ITEM_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace media_message_center {

class MediaNotificationView;

// MediaNotificationItem manages hiding/showing a MediaNotificationView.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationItem {
 public:
  // The name of the histogram used when recording user actions.
  static const char kUserActionHistogramName[];

  // The name of the histogram used when recording the source.
  static const char kSourceHistogramName[];

  // The source of the media session. This is used in metrics so new values must
  // only be added to the end.
  enum class Source {
    kUnknown,
    kWeb,
    kAssistant,
    kArc,
    kMaxValue = kArc,
  };

  MediaNotificationItem() = default;
  MediaNotificationItem(const MediaNotificationItem&) = delete;
  MediaNotificationItem& operator=(const MediaNotificationItem&) = delete;
  virtual ~MediaNotificationItem() = default;

  // Called by MediaNotificationView when created or destroyed.
  virtual void SetView(MediaNotificationView* view) = 0;

  // Called by MediaNotificationView when a button is pressed.
  virtual void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) = 0;

  // Hides the media notification.
  virtual void Dismiss() = 0;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ITEM_H_
