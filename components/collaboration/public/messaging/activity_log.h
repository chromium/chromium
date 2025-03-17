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

  // Explicit display metadata to be shown in the UI. These strings are common
  // across all platforms and hence generated in the service.
  // Text to be shown as title of the activity log item.
  std::u16string title_text;

  // Text to be shown as description of the activity log item.
  std::u16string description_text;

  // Text to be shown as relative time duration (e.g. 8 hours ago) of when
  // the event happened.
  std::u16string time_delta_text;

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

  // An optional tab ID. If set, returns activity log only for the particular
  // tab. Currently used in desktop only.
  std::optional<tab_groups::LocalTabID> local_tab_id;

  // Max number of rows to be shown in the activity log UI.
  int result_length;
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_PUBLIC_MESSAGING_ACTIVITY_LOG_H_
