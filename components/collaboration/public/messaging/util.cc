// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/messaging/util.h"

#include "base/check.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace collaboration::messaging {

namespace {

// Whether the persistent message is about a shared tab group that is no longer
// available to the current user.
bool IsAboutUnavailableTabGroup(PersistentMessage message) {
  if (message.type != PersistentNotificationType::TOMBSTONED) {
    return false;
  }
  return message.collaboration_event == CollaborationEvent::TAB_GROUP_REMOVED;
}

}  // namespace

std::optional<std::string> GetRemovedCollaborationsSummary(
    std::vector<PersistentMessage> messages) {
  // Find out how many messages that are about removed collaborations.
  size_t removed_collabs_count = 0;
  // Keep the first two removed collaborations titles.
  std::vector<std::string> titles(2);
  for (const auto& message : messages) {
    if (IsAboutUnavailableTabGroup(message)) {
      // Store only the first two titles.
      if (removed_collabs_count < 2) {
        CHECK(message.attribution.tab_group_metadata.has_value());
        TabGroupMessageMetadata metadata =
            message.attribution.tab_group_metadata.value();
        if (metadata.last_known_title.has_value()) {
          titles[removed_collabs_count] = metadata.last_known_title.value();
        }
      }
      // Count all groups.
      removed_collabs_count++;
    }
  }

  // Create the summary, as needed.
  switch (removed_collabs_count) {
    case 0:
      return std::nullopt;
    case 1:
      if (titles[0].length() > 0) {
        return l10n_util::GetStringFUTF8(
            IDS_COLLABORATION_ONE_GROUP_REMOVED_NOTIFICATION,
            base::UTF8ToUTF16(titles[0]));
      } else {
        return l10n_util::GetStringUTF8(
            IDS_COLLABORATION_ONE_UNNAMED_GROUP_REMOVED_NOTIFICATION);
      }
    case 2:
      if (titles[0].length() > 0 && titles[1].length() > 0) {
        return l10n_util::GetStringFUTF8(
            IDS_COLLABORATION_TWO_GROUPS_REMOVED_NOTIFICATION,
            base::UTF8ToUTF16(titles[0]), base::UTF8ToUTF16(titles[1]));
      } else {
        return l10n_util::GetStringFUTF8(
            IDS_COLLABORATION_SEVERAL_GROUPS_REMOVED_NOTIFICATION,
            base::FormatNumber(2));
      }
    default:
      return l10n_util::GetStringFUTF8(
          IDS_COLLABORATION_SEVERAL_GROUPS_REMOVED_NOTIFICATION,
          base::FormatNumber(removed_collabs_count));
  }
}

}  // namespace collaboration::messaging
