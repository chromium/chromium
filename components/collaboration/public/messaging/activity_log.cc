// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/messaging/activity_log.h"

#include "components/collaboration/public/messaging/message.h"

namespace collaboration::messaging {
namespace {

// Default value for the max number of rows to be shown on the activity log UI.
constexpr int kDefaultMaxRowsActivityLog = 5;

}  // namespace

// TODO(crbug.com/345856704): Update this to UNDEFINED.
ActivityLogItem::ActivityLogItem()
    : collaboration_event(CollaborationEvent::TAB_ADDED) {}

ActivityLogItem::ActivityLogItem(const ActivityLogItem& other) = default;

ActivityLogItem::~ActivityLogItem() = default;

ActivityLogQueryParams::ActivityLogQueryParams()
    : result_length(kDefaultMaxRowsActivityLog) {}

ActivityLogQueryParams::ActivityLogQueryParams(
    const ActivityLogQueryParams& other) = default;

ActivityLogQueryParams::~ActivityLogQueryParams() = default;

}  // namespace collaboration::messaging
