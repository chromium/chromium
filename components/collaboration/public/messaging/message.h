// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_MESSAGE_H_
#define COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_MESSAGE_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"

namespace collaboration::messaging {

// Actions that have been taken.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.collaboration.messaging)
enum class CollaborationEvent {
  // Used for messages such as an implicitly dirty tab group.
  UNDEFINED,

  // Source: TabGroupSyncService data.
  TAB_ADDED,
  TAB_REMOVED,
  TAB_UPDATED,
  TAB_GROUP_ADDED,
  TAB_GROUP_NAME_UPDATED,
  TAB_GROUP_COLOR_UPDATED,
  TAB_GROUP_REMOVED,

  // Source: DataSharingService data.
  COLLABORATION_ADDED,
  COLLABORATION_MEMBER_ADDED,
  COLLABORATION_MEMBER_REMOVED,
  // Deprecated: Migrated to TAB_GROUP_REMOVED instead. Current user left or
  // lost access.
  COLLABORATION_REMOVED,
};

// Different types of instant notifications that need to be shown immediately
// (and only once) to the user.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.collaboration.messaging)
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
//   org.chromium.components.collaboration.messaging)
enum class InstantNotificationType {
  UNDEFINED,
  // A special notification when a tab is removed while the user is focused on
  // the tab.
  CONFLICT_TAB_REMOVED,
};

// Different types of persistent notifications that need to be shown to the
// user.
// GENERATED_JAVA_ENUM_PACKAGE: (
//   org.chromium.components.collaboration.messaging)
enum class PersistentNotificationType {
  UNDEFINED,
  // A chip displayed for a specific tab. Only used in desktop.
  CHIP,
  // A marker that a tab has been changed and the user has not seen it yet.
  DIRTY_TAB,
  // A marker that one or more tabs in the tab group has changed and the user
  // has not seen it yet.
  DIRTY_TAB_GROUP,
  // A marker that an entity (tab or tab group) has been deleted and the user
  // has not seen it yet.
  TOMBSTONED,
  // The message was an instant message.
  INSTANT_MESSAGE,
};

// Metadata about the tab group a message is attributed to.
struct TabGroupMessageMetadata {
 public:
  TabGroupMessageMetadata();
  TabGroupMessageMetadata(const TabGroupMessageMetadata& other);
  ~TabGroupMessageMetadata();

  // The tab group this message is associated with (if any).
  std::optional<tab_groups::LocalTabGroupID> local_tab_group_id;

  // The tab group sync GUID this message is associated with (if any).
  std::optional<base::Uuid> sync_tab_group_id;

  // In the case where the tab group is no longer available, this contains the
  // last known title.
  std::optional<std::string> last_known_title;

  // In the case where the tab group is no longer available, this contains the
  // last known color.
  std::optional<tab_groups::TabGroupColorId> last_known_color;
};

// Metadata about the tab a message is attributed to.
struct TabMessageMetadata {
 public:
  TabMessageMetadata();
  TabMessageMetadata(const TabMessageMetadata& other);
  ~TabMessageMetadata();

  // The tab this message is associated with (if any).
  std::optional<tab_groups::LocalTabID> local_tab_id;

  // The sync GUID of the tab this message is associated with (if any).
  std::optional<base::Uuid> sync_tab_id;

  // In the case where the tab is no longer available, this contains the last
  // known URL (or empty string if unknown).
  std::optional<std::string> last_known_url;

  // In the case where the tab is no longer available, this contains the last
  // known title (or empty string if unknown).
  std::optional<std::string> last_known_title;

  // URL of the tab before the last navigation. Only populated for the tab
  // update events.
  std::optional<std::string> previous_url;
};

// A list of attribution data for a message, which can be used to associate it
// with particular tabs, tab groups, or people.
struct MessageAttribution {
 public:
  MessageAttribution();
  MessageAttribution(const MessageAttribution& other);
  ~MessageAttribution();

  // The id of this message. Non-empty if there is a corresponding entry in the
  // database, empty for synthetic messages.
  std::optional<base::Uuid> id;

  // The collaboration this message is associated with (if any).
  data_sharing::GroupId collaboration_id;

  // Metadata about the relevant tab group.
  std::optional<TabGroupMessageMetadata> tab_group_metadata;

  // Metadata about the relevant tab.
  std::optional<TabMessageMetadata> tab_metadata;

  // The user the related action applies to (if any).
  // This is the added or removed user for
  // CollaborationEvent::COLLABORATION_MEMBER_ADDED and
  // CollaborationEvent::COLLABORATION_MEMBER_REMOVED, otherwise std::nullopt.
  std::optional<data_sharing::GroupMember> affected_user;

  // Whether the affected user is same as the currently signed in user.
  bool affected_user_is_self = false;

  // The user who performed the related action and caused the message (if any).
  // This is not set for CollaborationEvent::COLLABORATION_MEMBER_ADDED and
  // CollaborationEvent::COLLABORATION_MEMBER_REMOVED since we do not have a way
  // to know how the change was triggered.
  std::optional<data_sharing::GroupMember> triggering_user;

  // Whether the triggering user is same as the currently signed in user.
  bool triggering_user_is_self = false;
};

// An instant notification that the UI to show something to the user
// immediately. Depending on the type of message, it might represent
// a single event or multiple events of similar type aggregated as a single
// message.
struct InstantMessage {
 public:
  InstantMessage();
  InstantMessage(const InstantMessage& other);
  ~InstantMessage();

  // The collaboration event associated with the message.
  CollaborationEvent collaboration_event;

  // The level of instant notification to show.
  InstantNotificationLevel level;

  // The type of instant notification to show.
  InstantNotificationType type;

  // The message content to be shown in the UI.
  std::u16string localized_message;

  // The list of message attributions for the messages that it represents.
  // For single message case, the size is 1. For aggregated message case, it
  // will be greater than 1.
  std::vector<MessageAttribution> attributions;
};

// A persistent notification that requires an ongoing UI affordance until
// certain conditions are met.
struct PersistentMessage {
 public:
  MessageAttribution attribution;

  // The collaboration event associated with the message.
  CollaborationEvent collaboration_event;

  // The type of persistent notification to show.
  PersistentNotificationType type;
};

// Helper method to query whether the message represents a non-aggregated
// message.
bool IsSingleMessage(const InstantMessage& message);

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_MESSAGE_H_
