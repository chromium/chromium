// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_util.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/views/controls/button/button.h"

namespace media_message_center {

using media_session::mojom::MediaSessionAction;

namespace {

// The action buttons in order of preference. If there is not enough space to
// show all the action buttons then this is used to determine which will be
// shown.
constexpr MediaSessionAction kPreferredActions[] = {
    MediaSessionAction::kPlay,
    MediaSessionAction::kPause,
    MediaSessionAction::kPreviousTrack,
    MediaSessionAction::kNextTrack,
    MediaSessionAction::kSeekBackward,
    MediaSessionAction::kSeekForward,
    MediaSessionAction::kEnterPictureInPicture,
    MediaSessionAction::kExitPictureInPicture,
};

// The maximum number of media notifications to count when recording the
// Media.Notification.Count histogram. 20 was chosen because it would be very
// unlikely to see a user with 20+ things playing at once.
const int kMediaNotificationCountHistogramMax = 20;

}  // namespace

const char kCountHistogramName[] = "Media.Notification.Count";
const char kCastCountHistogramName[] = "Media.Notification.Cast.Count";

std::u16string GetAccessibleNameFromMetadata(
    media_session::MediaMetadata session_metadata) {
  std::vector<std::u16string> text;

  if (!session_metadata.title.empty())
    text.push_back(session_metadata.title);

  if (!session_metadata.artist.empty())
    text.push_back(session_metadata.artist);

  if (!session_metadata.album.empty())
    text.push_back(session_metadata.album);

  std::u16string accessible_name = base::JoinString(text, u" - ");
  return accessible_name;
}

base::flat_set<MediaSessionAction> GetTopVisibleActions(
    const base::flat_set<MediaSessionAction>& enabled_actions,
    const base::flat_set<MediaSessionAction>& ignored_actions,
    size_t max_actions) {
  base::flat_set<MediaSessionAction> visible_actions;

  for (auto& action : kPreferredActions) {
    if (visible_actions.size() >= max_actions)
      break;

    if (!base::Contains(enabled_actions, action) ||
        base::Contains(ignored_actions, action))
      continue;

    visible_actions.insert(action);
  }

  return visible_actions;
}

MediaSessionAction GetActionFromButtonTag(const views::Button& button) {
  return static_cast<MediaSessionAction>(button.tag());
}

MediaSessionAction GetPlayPauseIgnoredAction(
    MediaSessionAction current_action) {
  return current_action == MediaSessionAction::kPlay
             ? MediaSessionAction::kPause
             : MediaSessionAction::kPlay;
}

MediaSessionAction GetPictureInPictureIgnoredAction(
    MediaSessionAction current_action) {
  return current_action == MediaSessionAction::kEnterPictureInPicture
             ? MediaSessionAction::kExitPictureInPicture
             : MediaSessionAction::kEnterPictureInPicture;
}

void RecordConcurrentNotificationCount(size_t count) {
  UMA_HISTOGRAM_EXACT_LINEAR(kCountHistogramName, count,
                             kMediaNotificationCountHistogramMax);
}

void RecordConcurrentCastNotificationCount(size_t count) {
  UMA_HISTOGRAM_EXACT_LINEAR(kCastCountHistogramName, count,
                             kMediaNotificationCountHistogramMax);
}

}  // namespace media_message_center
