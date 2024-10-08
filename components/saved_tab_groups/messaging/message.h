// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_MESSAGING_MESSAGE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_MESSAGING_MESSAGE_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_id.h"

namespace tab_groups::messaging {

// Actions that have been taken.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.tab_group_sync.messaging)
enum class UserAction {
  // Source: TabGroupSyncService data.
  TAB_ADDED,
  // TODO(345856704): How do we get attribution data here?
  TAB_REMOVED,
  TAB_NAVIGATED,

  // Source: DataSharingService data.
  COLLABORATION_USER_JOINED,
  COLLABORATION_USER_LEFT,
  // User left or lost access.
  COLLABORATION_REMOVED,
};

// Different types of instant notifications that need to be shown immediately
// (and only once) to the user.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.tab_group_sync.messaging)
enum class InstantNotificationLevel {
  UNDEFINED,
  // Show notification using OS notification.
  SYSTEM,
  // Show a browser level notification
  BROWSER,
};

// The notification type provides an explicit hint to the frontend about how
// they need to handle the notification, instead of the frontend needing to
// infer what it needs to do. Fallback option here is `UNDEFINED` for either
// trivial or easily implicit notification types.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.tab_group_sync.messaging)
enum class InstantNotificationType {
  UNDEFINED,
  // A special notification when a tab is removed while the user is focused on
  // the tab.
  CONFLICT_TAB_REMOVED,
};

// Different types of persistent notifications that need to be shown to the
// user.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.tab_group_sync.messaging)
enum class PersistentNotificationType {
  UNDEFINED,
  // Message displayed when the given item is visible (e.g. tab chip).
  FOCUSED,
  // A marker that a tab has been changed and the user has not seen it yet.
  DIRTY_TAB,
  // A marker that something in the tab group has changed and the user has not
  // seen it yet.
  DIRTY_TAB_GROUP,
};

// A list of attribution data for a message, which can be used to associate it
// with particular tabs, tab groups, or people.
struct MessageAttribution {
 public:
  MessageAttribution();
  MessageAttribution(const MessageAttribution& other);
  ~MessageAttribution();

  // TODO(nyquist): Maybe make collaboration, tab, group, and affected users
  // vectors.

  // The collaboration this message is associated with (if any).
  data_sharing::GroupId collaboration_id;

  // The tab group this message is associated with (if any).
  std::optional<LocalTabGroupID> local_tab_group_id;

  // The tab group sync GUID this message is associated with (if any).
  std::optional<base::Uuid> sync_tab_group_id;

  // The tab this message is associated with (if any).
  std::optional<LocalTabID> local_tab_id;

  // The sync GUID of the tab this message is associated with (if any).
  std::optional<base::Uuid> sync_tab_id;

  // The user the related action applies to (if any).
  std::optional<data_sharing::GroupMember> affected_user;

  // The user who performed the related action and caused the message (if any).
  std::optional<data_sharing::GroupMember> triggering_user;
};

// An instant notification that the UI to show something to the user
// immediately.
struct InstantMessage {
 public:
  MessageAttribution attribution;

  // The type of action associated with the message.
  UserAction action;

  // The level of instant notification to show.
  InstantNotificationLevel level;

  // The type of instant notification to show.
  InstantNotificationType type;
};

// A persistent notification that requires an ongoing UI affordance until
// certain conditions are met.
struct PersistentMessage {
 public:
  MessageAttribution attribution;

  // The type of action associated with the message.
  UserAction action;

  // The type of persistent notification to show.
  PersistentNotificationType type;
};

}  // namespace tab_groups::messaging

#endif  // COMPONENTS_SAVED_TAB_GROUPS_MESSAGING_MESSAGE_H_
