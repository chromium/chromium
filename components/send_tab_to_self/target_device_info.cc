// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/target_device_info.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/trace_event/trace_event.h"
#include "components/send_tab_to_self/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info.h"
#include "ui/base/l10n/l10n_util.h"

namespace send_tab_to_self {

TargetDeviceInfo::TargetDeviceInfo() = default;

TargetDeviceInfo::TargetDeviceInfo(std::string device_name,
                                   std::string cache_guid,
                                   syncer::DeviceInfo::FormFactor form_factor,
                                   syncer::DeviceInfo::OsType os_type,
                                   base::Time last_updated_timestamp,
                                   bool has_high_precision_timestamp)
    : device_name(std::move(device_name)),
      cache_guid(std::move(cache_guid)),
      form_factor(form_factor),
      os_type(os_type),
      last_updated_timestamp(last_updated_timestamp),
      has_high_precision_timestamp(has_high_precision_timestamp) {}

TargetDeviceInfo::TargetDeviceInfo(const TargetDeviceInfo& other) = default;
TargetDeviceInfo::~TargetDeviceInfo() = default;

bool TargetDeviceInfo::operator==(const TargetDeviceInfo& rhs) const {
  return device_name == rhs.device_name && cache_guid == rhs.cache_guid &&
         form_factor == rhs.form_factor && os_type == rhs.os_type &&
         last_updated_timestamp == rhs.last_updated_timestamp &&
         has_high_precision_timestamp == rhs.has_high_precision_timestamp;
}

std::u16string TargetDeviceInfo::GetLastActiveTimeForDisplay() const {
  const base::TimeDelta delta =
      std::max(base::TimeDelta(), base::Time::Now() - last_updated_timestamp);

  if (base::FeatureList::IsEnabled(kSendTabToSelfImprovedLastActiveLabels) &&
      has_high_precision_timestamp) {
    if (delta < base::Minutes(1)) {
      return l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_NOW);
    }

    if (delta < base::Hours(1)) {
      return l10n_util::GetPluralStringFUTF16(
          IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, delta.InMinutes());
    }

    if (delta < base::Days(1)) {
      return l10n_util::GetPluralStringFUTF16(
          IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS, delta.InHours());
    }
  }

  return l10n_util::GetPluralStringFUTF16(
      IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, delta.InDays());
}

}  // namespace send_tab_to_self
