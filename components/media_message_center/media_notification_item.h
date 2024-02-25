// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ITEM_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ITEM_H_

#include <optional>

#include "base/component_export.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace base {
class UnguessableToken;
}  // namespace base

namespace media_message_center {

// The source of the media item. This is used in metrics so new values must only
// be added to the end.
enum class Source {
  kUnknown,
  kWeb,
  kAssistant,
  kArc,
  kLocalCastSession,
  kNonLocalCastSession,
  kCastDevicePicker,
  kMaxValue = kCastDevicePicker,
};

// The source type of the media item.
enum class SourceType {
  kLocalMediaSession,
  kCast,
  kPresentationRequest,
  kMaxValue = kPresentationRequest,
};

class MediaNotificationView;

// MediaNotificationItem manages hiding/showing a MediaNotificationView.
class COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) MediaNotificationItem {
 public:
  // The name of the histogram used when recording user actions.
  static const char kUserActionHistogramName[];

  // The name of the histogram used when recording user actions for Cast
  // notifications.
  static const char kCastUserActionHistogramName[];

  // The name of the histogram used when recording the source.
  static const char kSourceHistogramName[];

  MediaNotificationItem() = default;
  MediaNotificationItem(const MediaNotificationItem&) = delete;
  MediaNotificationItem& operator=(const MediaNotificationItem&) = delete;
  virtual ~MediaNotificationItem() = default;

  // Called by MediaNotificationView when created or destroyed.
  virtual void SetView(MediaNotificationView* view) = 0;

  // Called by MediaNotificationView when a button is pressed.
  virtual void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) = 0;

  // Called by MediaNotificationViewImpl when progress bar is clicked to seek.
  virtual void SeekTo(base::TimeDelta time) = 0;

  // Hides the media notification.
  virtual void Dismiss() = 0;

  // Called by MediaNotificationView when volume is set.
  virtual void SetVolume(float volume) = 0;

  // Called by MediaNotificationView when mute button is clicked.
  virtual void SetMute(bool mute) = 0;

  // Called by MediaNotificationService when a Remote Playback session is
  // started.
  virtual bool RequestMediaRemoting() = 0;

  // Returns the source of the media item for recording metrics.
  virtual media_message_center::Source GetSource() const = 0;

  // Returns the source type of the media item.
  virtual media_message_center::SourceType GetSourceType() const = 0;

  // Returns the ID of the source of the media session, if it has one.
  virtual std::optional<base::UnguessableToken> GetSourceId() const = 0;
};

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ITEM_H_
