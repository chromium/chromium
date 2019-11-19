// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONSTANTS_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONSTANTS_H_

#include "base/component_export.h"

namespace media_message_center {

// The minimum size in px that the media session artwork can be to be displayed
// in the notification.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER)
extern const int kMediaSessionNotificationArtworkMinSize;

// The desired size in px for the media session artwork to be displayed in the
// notification. The media session service will try and select artwork closest
// to this size.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER)
extern const int kMediaSessionNotificationArtworkDesiredSize;

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_CONSTANTS_H_
