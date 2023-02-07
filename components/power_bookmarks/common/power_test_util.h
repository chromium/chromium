// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_TEST_UTIL_H_
#define COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_TEST_UTIL_H_

#include "components/power_bookmarks/common/power.h"

namespace power_bookmarks {
std::unique_ptr<Power> MakePower(
    GURL url,
    sync_pb::PowerBookmarkSpecifics::PowerType power_type,
    base::Time time_modified = base::Time(),
    std::unique_ptr<sync_pb::PowerEntity> power_specifics =
        std::make_unique<sync_pb::PowerEntity>());

std::unique_ptr<Power> MakePower(
    GURL url,
    sync_pb::PowerBookmarkSpecifics::PowerType power_type,
    std::unique_ptr<sync_pb::PowerEntity> power_specifics);

std::unique_ptr<Power> MakePower(GURL url, base::Time time_modified);
}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_TEST_UTIL_H_
