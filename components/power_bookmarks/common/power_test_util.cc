// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/common/power_test_util.h"

namespace power_bookmarks {

std::unique_ptr<Power> MakePower(
    GURL url,
    sync_pb::PowerBookmarkSpecifics::PowerType power_type,
    base::Time time_modified,
    std::unique_ptr<sync_pb::PowerEntity> power_specifics) {
  std::unique_ptr<Power> power =
      std::make_unique<Power>(std::move(power_specifics));
  power->set_guid(base::Uuid::GenerateRandomV4());
  power->set_url(url);
  power->set_power_type(power_type);
  power->set_time_modified(time_modified);
  return power;
}

std::unique_ptr<Power> MakePower(
    GURL url,
    sync_pb::PowerBookmarkSpecifics::PowerType power_type,
    std::unique_ptr<sync_pb::PowerEntity> power_specifics) {
  return MakePower(url, power_type, base::Time(), std::move(power_specifics));
}

std::unique_ptr<Power> MakePower(GURL url, base::Time time_modified) {
  return MakePower(url, sync_pb::PowerBookmarkSpecifics::POWER_TYPE_MOCK,
                   time_modified);
}

}  // namespace power_bookmarks
