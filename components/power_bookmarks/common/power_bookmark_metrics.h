// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_BOOKMARK_METRICS_H_
#define COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_BOOKMARK_METRICS_H_

#include "components/sync/protocol/power_bookmark_specifics.pb.h"

namespace power_bookmarks::metrics {

void RecordPowerCreated(sync_pb::PowerBookmarkSpecifics::PowerType power_type,
                        bool success);

void RecordPowerUpdated(sync_pb::PowerBookmarkSpecifics::PowerType power_type,
                        bool success);

void RecordPowerDeleted(bool success);

void RecordPowersDeletedForURL(
    sync_pb::PowerBookmarkSpecifics::PowerType power_type,
    bool success);

void RecordDatabaseError(int error);

void RecordDatabaseSizeAtStartup(int64_t size_in_bytes);

}  // namespace power_bookmarks::metrics

#endif  // COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_BOOKMARK_METRICS_H_