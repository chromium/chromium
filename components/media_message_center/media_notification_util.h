// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_UTIL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_UTIL_H_

#include "base/component_export.h"
#include "url/origin.h"

namespace media_message_center {

// The name of the histogram used to record the number of concurrent media
// notifications.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) extern const char kCountHistogramName[];

// Checks if the origin has a human-friendly url.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER)
bool IsOriginGoodForDisplay(const url::Origin& origin);

// Creates a string formatting a url::Origin in a human-friendly way.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER)
std::u16string GetOriginNameForDisplay(const url::Origin& origin);

// Records the concurrent number of media notifications displayed.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER)
void RecordConcurrentNotificationCount(size_t count);

// Records the concurrent number of Cast media notifications displayed.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER)
void RecordConcurrentCastNotificationCount(size_t count);

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_UTIL_H_
