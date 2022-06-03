// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_UTIL_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_UTIL_H_

#include <string>

#include "base/time/time.h"

namespace sync_pb {
class DeviceInfoSpecifics;
}

namespace syncer {

// This class contains constants and static helper functions that are shared
// between DeviceInfoSyncService and DeviceInfoSyncBridge.
class DeviceInfoUtil {
 public:
  // The prefix to be appended before the cache guid when creating tags. This is
  // needed for backwards compatiblity with all old clients because we need to
  // ensure everyone uses the same logic to generate the tag, so that everyone
  // can independently hash the tag to the same value.
  static const char kClientTagPrefix[];

  // The amount of time a device can go without an updates before we consider it
  // stale/inactive, and start ignoring it for active device counts.
  static const base::TimeDelta kActiveThreshold;

  // The interval with which this device is updated to the sync servers if
  // online and while sync is actively running (e.g. excludes backgrounded apps
  // on Android).
  static base::TimeDelta GetPulseInterval();

  // Determines the amount of time to wait before pulsing something with the
  // given |last_update| timestamp. This uses the current time from |now| along
  // with |GetPulseInterval()|, and will never return a negative delay.
  // The smallest delay this function will return, even for something extremely
  // old will be a delay of 0 time units.
  static base::TimeDelta CalculatePulseDelay(const base::Time last_update,
                                             const base::Time now);

  // Determines if the given |last_update| timestamp should be considered
  // active based on |kActiveThreshold|, given the current time.
  static bool IsActive(const base::Time last_update, const base::Time now);

  // Formats ClientTag from DeviceInfoSpecifics.
  static std::string SpecificsToTag(
      const sync_pb::DeviceInfoSpecifics& specifics);

  // Extracts cache_guid from ClientTag.
  static std::string TagToCacheGuid(const std::string& tag);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_UTIL_H_
