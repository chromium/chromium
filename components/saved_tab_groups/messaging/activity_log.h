// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_MESSAGING_ACTIVITY_LOG_H_
#define COMPONENTS_SAVED_TAB_GROUPS_MESSAGING_ACTIVITY_LOG_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/uuid.h"
#include "components/data_sharing/public/group_data.h"
#include "components/saved_tab_groups/messaging/message.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_id.h"

namespace tab_groups::messaging {

// Struct containing information needed to show one row in the activity log UI.
struct ActivityLogItem {
 public:
  ActivityLogItem();
  ActivityLogItem(const ActivityLogItem& other);
  ~ActivityLogItem();

  // Explicit display metadata to be shown in the UI.
  std::string title_text;
  std::string description_text;
  std::string timestamp_text;

  // The type of action associated with the log item.
  UserAction user_action_type;

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

}  // namespace tab_groups::messaging

#endif  // COMPONENTS_SAVED_TAB_GROUPS_MESSAGING_ACTIVITY_LOG_H_
