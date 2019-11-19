// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_UTIL_H_
#define COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_UTIL_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "services/media_session/public/mojom/media_session.mojom.h"

namespace views {
class Button;
}

namespace media_message_center {

// The name of the histogram used to record the number of concurrent media
// notifications.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER) extern const char kCountHistogramName[];

// Creates a string describing media session metadata intended to be read out by
// a screen reader.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER)
base::string16 GetAccessibleNameFromMetadata(
    media_session::MediaMetadata session_metadata);

// Returns actions that can be displayed as buttons in the media controller UI
// from the set (|enabled_actions| - |ignored_actions|). This will return at
// most |max_actions| - if needed, the actions will the least priority will be
// dropped.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER)
base::flat_set<media_session::mojom::MediaSessionAction> GetTopVisibleActions(
    const base::flat_set<media_session::mojom::MediaSessionAction>&
        enabled_actions,
    const base::flat_set<media_session::mojom::MediaSessionAction>&
        ignored_actions,
    size_t max_actions);

// Returns the |MediaSessionAction| corresponding to playback action |button|.
// |button| is expected to be tagged with a |MediaSessionAction|.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER)
media_session::mojom::MediaSessionAction GetActionFromButtonTag(
    const views::Button& button);

// Returns the action on the play/pause toggle button that should be
// ignored when calculating the visible actions.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER)
media_session::mojom::MediaSessionAction GetPlayPauseIgnoredAction(
    media_session::mojom::MediaSessionAction current_action);

// Records the concurrent number of media notifications displayed.
COMPONENT_EXPORT(MEDIA_MESSAGE_CENTER)
void RecordConcurrentNotificationCount(size_t count);

}  // namespace media_message_center

#endif  // COMPONENTS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_UTIL_H_
