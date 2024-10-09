// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/messaging/activity_log.h"

#include "components/saved_tab_groups/messaging/message.h"

namespace tab_groups::messaging {
namespace {

// Default value for the max number of rows to be shown on the activity log UI.
constexpr int kDefaultMaxRowsActivityLog = 5;

}  // namespace

// TODO(crbug.com/345856704): Update this to UNDEFINED.
ActivityLogItem::ActivityLogItem() : user_action_type(UserAction::TAB_ADDED) {}

ActivityLogItem::ActivityLogItem(const ActivityLogItem& other) = default;

ActivityLogItem::~ActivityLogItem() = default;

ActivityLogQueryParams::ActivityLogQueryParams()
    : result_length(kDefaultMaxRowsActivityLog) {}

ActivityLogQueryParams::ActivityLogQueryParams(
    const ActivityLogQueryParams& other) = default;

ActivityLogQueryParams::~ActivityLogQueryParams() = default;

}  // namespace tab_groups::messaging
