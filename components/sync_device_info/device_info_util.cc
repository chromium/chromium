// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_util.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_util.h"
#include "components/sync/protocol/device_info_specifics.pb.h"

namespace syncer {

const char DeviceInfoUtil::kClientTagPrefix[] = "DeviceInfo_";
const base::TimeDelta DeviceInfoUtil::kActiveThreshold = base::Days(14);

namespace {

// Feature flag for configuring the pulse interval.
// TODO(crbug.com/1045940): Remove this when the experiment concludes.
const base::Feature kPulseInterval{"PulseInterval",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// The delay between periodic updates to the entry corresponding to this device.
const base::FeatureParam<int> kPulseIntervalMinutes = {
    &kPulseInterval, "PulseIntervalMinutes", 1440};  // 1 day

base::TimeDelta Age(const base::Time last_update, const base::Time now) {
  // Don't allow negative age for things somehow updated in the future.
  return std::max(base::TimeDelta(), now - last_update);
}

}  // namespace

// static
base::TimeDelta DeviceInfoUtil::GetPulseInterval() {
  return base::Minutes(kPulseIntervalMinutes.Get());
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
