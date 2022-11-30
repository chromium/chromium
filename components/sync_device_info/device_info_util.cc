// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_util.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/sync/protocol/device_info_specifics.pb.h"

namespace syncer {

const char DeviceInfoUtil::kClientTagPrefix[] = "DeviceInfo_";
const base::TimeDelta DeviceInfoUtil::kActiveThreshold = base::Days(14);

namespace {

// The delay between periodic updates to the entry corresponding to this device.
const base::TimeDelta kPulseInterval = base::Days(1);

base::TimeDelta Age(const base::Time last_update, const base::Time now) {
  // Don't allow negative age for things somehow updated in the future.
  return std::max(base::TimeDelta(), now - last_update);
}

}  // namespace

// static
base::TimeDelta DeviceInfoUtil::GetPulseInterval() {
  return kPulseInterval;
}

// static
base::TimeDelta DeviceInfoUtil::CalculatePulseDelay(
    const base::Time last_update,
    const base::Time now) {
  // Don't allow negative delays for very stale data, use delay of 0.
  return std::max(base::TimeDelta(),
                  GetPulseInterval() - Age(last_update, now));
}

// static
bool DeviceInfoUtil::IsActive(const base::Time last_update,
                              const base::Time now) {
  return Age(last_update, now) < kActiveThreshold;
}

// static
std::string DeviceInfoUtil::SpecificsToTag(
    const sync_pb::DeviceInfoSpecifics& specifics) {
  return kClientTagPrefix + specifics.cache_guid();
}

// static
std::string DeviceInfoUtil::TagToCacheGuid(const std::string& tag) {
  DCHECK(base::StartsWith(tag, kClientTagPrefix, base::CompareCase::SENSITIVE));
  return tag.substr(strlen(kClientTagPrefix));
}

}  // namespace syncer
