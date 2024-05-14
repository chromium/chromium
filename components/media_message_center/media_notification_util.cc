// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_message_center/media_notification_util.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/media_message_center/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
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

bool IsOriginGoodForDisplay(const url::Origin& origin) {
  return !origin.opaque() ||
         origin.GetTupleOrPrecursorTupleIfOpaque().IsValid();
}

std::u16string GetOriginNameForDisplay(const url::Origin& origin) {
  const auto url = origin.opaque()
                       ? origin.GetTupleOrPrecursorTupleIfOpaque().GetURL()
                       : origin.GetURL();
  return url_formatter::FormatUrlForSecurityDisplay(
      url, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
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

const gfx::VectorIcon* GetVectorIconForMediaAction(MediaSessionAction action) {
  switch (action) {
    case MediaSessionAction::kPreviousTrack:
      return &kMediaPreviousTrackIcon;
    case MediaSessionAction::kSeekBackward:
      return &kMediaSeekBackwardIcon;
    case MediaSessionAction::kPlay:
      return &kPlayArrowIcon;
    case MediaSessionAction::kPause:
      return &kPauseIcon;
    case MediaSessionAction::kSeekForward:
      return &kMediaSeekForwardIcon;
    case MediaSessionAction::kNextTrack:
      return &kMediaNextTrackIcon;
    case MediaSessionAction::kEnterPictureInPicture:
      return &kMediaEnterPipIcon;
    case MediaSessionAction::kExitPictureInPicture:
      return &kMediaExitPipIcon;
    case MediaSessionAction::kStop:
    case MediaSessionAction::kSkipAd:
    case MediaSessionAction::kSeekTo:
    case MediaSessionAction::kScrubTo:
    case MediaSessionAction::kSwitchAudioDevice:
    case MediaSessionAction::kToggleMicrophone:
    case MediaSessionAction::kToggleCamera:
    case MediaSessionAction::kHangUp:
    case MediaSessionAction::kRaise:
    case MediaSessionAction::kSetMute:
    case MediaSessionAction::kPreviousSlide:
    case MediaSessionAction::kNextSlide:
    case MediaSessionAction::kEnterAutoPictureInPicture:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return nullptr;
}

const std::u16string GetAccessibleNameForMediaAction(
    MediaSessionAction action) {
  switch (action) {
    case MediaSessionAction::kPreviousTrack:
      return l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PREVIOUS_TRACK);
    case MediaSessionAction::kSeekBackward:
      return l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SEEK_BACKWARD);
    case MediaSessionAction::kPlay:
      return l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PLAY);
    case MediaSessionAction::kPause:
      return l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_PAUSE);
    case MediaSessionAction::kSeekForward:
      return l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_SEEK_FORWARD);
    case MediaSessionAction::kNextTrack:
      return l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_NEXT_TRACK);
    case MediaSessionAction::kEnterPictureInPicture:
      return l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_ENTER_PIP);
    case MediaSessionAction::kExitPictureInPicture:
      return l10n_util::GetStringUTF16(
          IDS_MEDIA_MESSAGE_CENTER_MEDIA_NOTIFICATION_ACTION_EXIT_PIP);
    case MediaSessionAction::kStop:
    case MediaSessionAction::kSkipAd:
    case MediaSessionAction::kSeekTo:
    case MediaSessionAction::kScrubTo:
    case MediaSessionAction::kSwitchAudioDevice:
    case MediaSessionAction::kToggleMicrophone:
    case MediaSessionAction::kToggleCamera:
    case MediaSessionAction::kHangUp:
    case MediaSessionAction::kRaise:
    case MediaSessionAction::kSetMute:
    case MediaSessionAction::kPreviousSlide:
    case MediaSessionAction::kNextSlide:
    case MediaSessionAction::kEnterAutoPictureInPicture:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return std::u16string();
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
