// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_ACTIVITY_LOG_H_
#define COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_ACTIVITY_LOG_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/collaboration/public/messaging/message.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_id.h"

namespace collaboration::messaging {

// Describes various types of actions that are taken when a recent activity row
// is clicked. Each row corresponds to one type of action.
//
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.collaboration.messaging
enum class RecentActivityAction {
  // No action should be taken.
  kNone = 0,

  // Focus a given tab.
  kFocusTab = 1,

  // Reopen a given URL as a new tab in the group.
  kReopenTab = 2,

  // Open the tab group dialog UI to edit tab group properties.
  kOpenTabGroupEditDialog = 3,

  // Open the people group management screen.
  kManageSharing = 4
};

// Struct containing information needed to show one row in the activity log UI.
struct ActivityLogItem {
 public:
  ActivityLogItem();
  ActivityLogItem(const ActivityLogItem& other);
  ~ActivityLogItem();

  // The type of event associated with the log item.
  CollaborationEvent collaboration_event;

  // Explicit display metadata to be shown in the UI.
  // Deprecated. Should be removed soon after the platforms have moved to use
  // the raw values instead of composed strings. The platform UI is responsible
  // to create the string to be shown.
  std::string title_text;
  std::string description_text;
  std::string timestamp_text;

  // Display name to be shown in the title line.
  // This is the triggering user for tab and tab group related events.
  // This is the affected user for membership changes (added/removed user).
  // This the `data_sharing::GroupMember::given_name`.
  std::string user_display_name;

  // Whether the user associated with the activity log item is the current
  // signed in user themselves.
  bool user_is_self = false;

  // Description text to be shown on first half of the description line. This
  // will be concatenated with the `time_delta` text. Can be empty string for
  // certain type of events in which case only `time_delta` is to be shown
  // without concatenation character.
  std::u16string description;

  // The time duration  that has passed since the action happened. Used for
  // generating the relative duration text that will be appended to the
  // description. If the description is empty, the entire description line will
  // contain only the relative duration without the concatenation character.
  base::TimeDelta time_delta;

  // Whether the favicon should be shown for this row. Only tab related updates
  // show a favicon.
  bool show_favicon = false;

  // The type of action to be taken when this activity row is clicked.
  RecentActivityAction action;

  // Implicit metadata that will be used to invoke the delegate when the
  // activity row is clicked.
  MessageAttribution activity_metadata;
};

// Query params for retrieving a list of rows to be shown on
// the activity log UI.
struct ActivityLogQueryParams {
  ActivityLogQueryParams();
  ActivityLogQueryParams(const ActivityLogQueryParams& other);
  ~ActivityLogQueryParams();

  // The collaboration associated with the activity log.
  data_sharing::GroupId collaboration_id;

  // Max number of rows to be shown in the activity log UI.
  int result_length;
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_ACTIVITY_LOG_H_
